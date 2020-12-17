// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_linux.h"

#include <fcntl.h>
#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle_usbfs.h"
#include "services/device/usb/usb_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"

namespace {
constexpr int kUsbClassMassStorage = 0x08;
}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace device {

UsbDeviceLinux::UsbDeviceLinux(const std::string& device_path,
                               std::unique_ptr<UsbDeviceDescriptor> descriptor)
    : UsbDevice(std::move(descriptor->device_info)),
      device_path_(device_path) {}

UsbDeviceLinux::~UsbDeviceLinux() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)

void UsbDeviceLinux::CheckUsbAccess(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  chromeos::PermissionBrokerClient::Get()->CheckPathAccess(device_path_,
                                                           std::move(callback));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void UsbDeviceLinux::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  uint32_t allowed_interfaces_mask = AllowedInterfacesMask();

  if (allowed_interfaces_mask == 0) {
    LOG(ERROR) << "Tried to open USB device with no allowed interfaces: "
               << device_path_;
    std::move(callback).Run(nullptr);
    return;
  }

  // create the pipe used as a lifetime to re-attach the original kernel driver
  // to the USB device in permission_broker.
  base::ScopedFD read_end, write_end;
  if (!base::CreatePipe(&read_end, &write_end, /*non_blocking*/ true)) {
    LOG(ERROR) << "Couldn't create pipe for USB device " << device_path_;
    std::move(callback).Run(nullptr);
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  chromeos::PermissionBrokerClient::Get()->ClaimDevicePath(
      device_path_, allowed_interfaces_mask, read_end.get(),
      base::BindOnce(&UsbDeviceLinux::OnOpenRequestComplete, this,
                     copyable_callback, std::move(write_end)),
      base::BindOnce(&UsbDeviceLinux::OnOpenRequestError, this,
                     copyable_callback));
#else
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      UsbService::CreateBlockingTaskRunner();
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceLinux::OpenOnBlockingThread, this,
                     std::move(callback), base::ThreadTaskRunnerHandle::Get(),
                     blocking_task_runner));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

uint32_t UsbDeviceLinux::AllowedInterfacesMask() {
  uint32_t result = 0;
  for (auto& configuration : device_info().configurations) {
    for (auto& interface : configuration->interfaces) {
      if (interface->interface_number >= 32) {
        LOG(ERROR) << "Interface number too high in USB descriptor.";
        continue;
      }

      bool has_mass_storage_interface = false;
      for (auto& alternate : interface->alternates) {
        has_mass_storage_interface |=
            alternate->class_code == kUsbClassMassStorage;
      }
      if (!has_mass_storage_interface)
        result |= (1U << interface->interface_number);
    }
  }

  return result;
}

void UsbDeviceLinux::OnOpenRequestComplete(OpenCallback callback,
                                           base::ScopedFD lifeline_fd,
                                           base::ScopedFD fd) {
  if (!fd.is_valid()) {
    USB_LOG(EVENT) << "Did not get valid device handle from permission broker.";
    std::move(callback).Run(nullptr);
    return;
  }
  Opened(std::move(fd), std::move(lifeline_fd), std::move(callback),
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
  if (fd.is_valid()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&UsbDeviceLinux::Opened, this, std::move(fd),
                                  base::ScopedFD(), std::move(callback),
                                  blocking_task_runner));
  } else {
    USB_PLOG(EVENT) << "Failed to open " << device_path_;
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback), nullptr));
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void UsbDeviceLinux::Opened(
    base::ScopedFD fd,
    base::ScopedFD lifeline_fd,
    OpenCallback callback,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<UsbDeviceHandle> device_handle = new UsbDeviceHandleUsbfs(
      this, std::move(fd), std::move(lifeline_fd), blocking_task_runner);
  handles().push_back(device_handle.get());
  std::move(callback).Run(device_handle);
}

}  // namespace device
