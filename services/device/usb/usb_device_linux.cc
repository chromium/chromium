// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_linux.h"

#include <fcntl.h>
#include <stddef.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle_usbfs.h"
#include "services/device/usb/usb_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/memory/memory_pressure_monitor.h"
#include "base/process/process_metrics.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#include "components/crash/core/common/crash_key.h"
#include "services/device/public/cpp/device_features.h"
namespace {
constexpr uint32_t kAllInterfacesMask = ~0U;
}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

UsbDeviceLinux::UsbDeviceLinux(const std::string& device_path,
                               std::unique_ptr<UsbDeviceDescriptor> descriptor)
    : UsbDevice(std::move(descriptor->device_info)),
      device_path_(device_path) {}

UsbDeviceLinux::~UsbDeviceLinux() = default;

#if BUILDFLAG(IS_CHROMEOS)

void UsbDeviceLinux::CheckUsbAccess(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chromeos::PermissionBrokerClient::Get()->CheckPathAccess(device_path_,
                                                           std::move(callback));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void UsbDeviceLinux::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kUsbDeviceLinuxOpenCrashKey)) {
    // Opening a USB device on ChromeOS sometimes crashes with signatures seen
    // in crbug.com/40069034 and crbug.com/332722607. The crash is caused by a
    // CHECK failure in MessageWriter::AppendBasic when
    // dbus_message_iter_append_basic fails (returns false). DBus documentation
    // indicates this only happens on OOM, but it can also happen on file
    // descriptor exhaustion.
    //
    // Record crash keys with the current memory pressure and open file
    // descriptor counts to aid in debugging these crashes.
    static crash_reporter::CrashKeyString<6> memory_pressure_critical(
        "memory-pressure-critical");
    static crash_reporter::CrashKeyString<12> open_fd_count("open-fd-count");
    static crash_reporter::CrashKeyString<12> open_fd_soft_limit(
        "open-fd-soft-limit");
    auto* memory_pressure_monitor = base::MemoryPressureMonitor::Get();
    if (memory_pressure_monitor) {
      memory_pressure_critical.Set(
          (memory_pressure_monitor->GetCurrentPressureLevel() ==
           base::MemoryPressureMonitor::MemoryPressureLevel::
               MEMORY_PRESSURE_LEVEL_CRITICAL)
              ? "true"
              : "false");
    }
    auto process_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
    if (process_metrics) {
      open_fd_count.Set(
          base::NumberToString(process_metrics->GetOpenFdCount()));
      open_fd_soft_limit.Set(
          base::NumberToString(process_metrics->GetOpenFdSoftLimit()));
    }
  }
  // create the pipe used as a lifetime to re-attach the original kernel driver
  // to the USB device in permission_broker.
  base::ScopedFD read_end, write_end;
  if (!base::CreatePipe(&read_end, &write_end, /*non_blocking*/ true)) {
    LOG(ERROR) << "Couldn't create pipe for USB device " << device_path_;
    std::move(callback).Run(nullptr);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  chromeos::PermissionBrokerClient::Get()->OpenPathAndRegisterClient(
      device_path_, kAllInterfacesMask, read_end.get(),
      base::BindOnce(&UsbDeviceLinux::OnOpenRequestComplete, this,
                     std::move(split_callback.first), std::move(write_end)),
      base::BindOnce(&UsbDeviceLinux::OnOpenRequestError, this,
                     std::move(split_callback.second)));
#else
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceLinux::OpenOnBlockingThread, this,
                     std::move(callback),
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     blocking_task_runner));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)

void UsbDeviceLinux::OnOpenRequestComplete(OpenCallback callback,
                                           base::ScopedFD lifeline_fd,
                                           const std::string& client_id,
                                           base::ScopedFD fd) {
  if (!fd.is_valid()) {
    USB_LOG(EVENT) << "Did not get valid device handle from permission broker.";
    std::move(callback).Run(nullptr);
    return;
  }
  Opened(std::move(fd), std::move(lifeline_fd), client_id, std::move(callback),
         UsbService::CreateBlockingTaskRunner());
}

void UsbDeviceLinux::OnOpenRequestError(OpenCallback callback,
                                        const std::string& error_name,
                                        const std::string& error_message) {
  USB_LOG(EVENT) << "Permission broker failed to open the device: "
                 << error_name << ": " << error_message;
  std::move(callback).Run(nullptr);
}

#else

void UsbDeviceLinux::OpenOnBlockingThread(
    OpenCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  base::ScopedFD fd(HANDLE_EINTR(open(device_path_.c_str(), O_RDWR)));
  // Client id is only used for ChromeOS so pass empty string here to indicate
  // an invalid client id.
  std::string empty_client_id = "";
  if (fd.is_valid()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&UsbDeviceLinux::Opened, this, std::move(fd),
                                  base::ScopedFD(), empty_client_id,
                                  std::move(callback), blocking_task_runner));
  } else {
    USB_PLOG(EVENT) << "Failed to open " << device_path_;
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), nullptr));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void UsbDeviceLinux::Opened(
    base::ScopedFD fd,
    base::ScopedFD lifeline_fd,
    const std::string& client_id,
    OpenCallback callback,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<UsbDeviceHandle> device_handle =
      new UsbDeviceHandleUsbfs(this, std::move(fd), std::move(lifeline_fd),
                               client_id, blocking_task_runner);
  handles().push_back(device_handle.get());
  std::move(callback).Run(device_handle);
}

}  // namespace device
