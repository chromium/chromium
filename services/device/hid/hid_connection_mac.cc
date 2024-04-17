// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_connection_mac.h"

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/hid/hid_connection_mac.h"
#include "services/device/hid/hid_service.h"

namespace device {

namespace {

std::string HexErrorCode(IOReturn error_code) {
  return base::StringPrintf("0x%04x", error_code);
}

}  // namespace

HidConnectionMac::HidConnectionMac(
    base::apple::ScopedCFTypeRef<IOHIDDeviceRef> device,
    scoped_refptr<HidDeviceInfo> device_info,
    bool allow_protected_reports,
    bool allow_fido_reports)
    : HidConnection(device_info, allow_protected_reports, allow_fido_reports),
      device_(std::move(device)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          HidService::kBlockingTaskTraits)) {
  IOHIDDeviceScheduleWithRunLoop(device_.get(), CFRunLoopGetMain(),
                                 kCFRunLoopDefaultMode);

  size_t expected_report_size = device_info->max_input_report_size();
  if (device_info->has_report_id())
    expected_report_size++;
  inbound_buffer_.resize(expected_report_size);

  if (inbound_buffer_.size() > 0) {
    AddRef();  // Hold a reference to this while this callback is registered.
    IOHIDDeviceRegisterInputReportCallback(
        device_.get(), &inbound_buffer_[0], inbound_buffer_.size(),
        &HidConnectionMac::InputReportCallback, this);
  }
}

HidConnectionMac::~HidConnectionMac() {}

void HidConnectionMac::PlatformClose() {
  if (inbound_buffer_.size() > 0) {
    IOHIDDeviceRegisterInputReportCallback(device_.get(), &inbound_buffer_[0],
                                           inbound_buffer_.size(), NULL, this);
    // Release the reference taken when this callback was registered.
    Release();
  }

  IOHIDDeviceUnscheduleFromRunLoop(device_.get(), CFRunLoopGetMain(),
                                   kCFRunLoopDefaultMode);
  IOReturn result = IOHIDDeviceClose(device_.get(), 0);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to close HID device: " << HexErrorCode(result);
  }
}

void HidConnectionMac::PlatformWrite(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&HidConnectionMac::SetReportAsync, this,
                     kIOHIDReportTypeOutput, buffer, std::move(callback)));
}

void HidConnectionMac::PlatformGetFeatureReport(uint8_t report_id,
                                                ReadCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HidConnectionMac::GetFeatureReportAsync, this,
                                report_id, std::move(callback)));
}

void HidConnectionMac::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&HidConnectionMac::SetReportAsync, this,
                     kIOHIDReportTypeFeature, buffer, std::move(callback)));
}

// static
void HidConnectionMac::InputReportCallback(void* context,
                                           IOReturn result,
                                           void* sender,
                                           IOHIDReportType type,
                                           uint32_t report_id,
                                           uint8_t* report_bytes_ptr,
                                           CFIndex report_bytes_len) {
  HidConnectionMac* connection = static_cast<HidConnectionMac*>(context);
  if (result != kIOReturnSuccess) {
    HID_LOG(EVENT) << "Failed to read input report: " << HexErrorCode(result);
    return;
  }

  // SAFETY: This function is called by macOS with the guarantee that
  // `report_byte_ptr` will point to at least `report_bytes_len` many bytes.
  base::span<const uint8_t> report_bytes = UNSAFE_BUFFERS(base::span(
      report_bytes_ptr, base::checked_cast<size_t>(report_bytes_len)));

  scoped_refptr<base::RefCountedBytes> buffer;
  if (connection->device_info()->has_report_id()) {
    // report_id is already contained in report_bytes
    buffer = base::MakeRefCounted<base::RefCountedBytes>(report_bytes);
  } else {
    buffer = base::MakeRefCounted<base::RefCountedBytes>(
        (base::CheckedNumeric<size_t>(report_bytes.size()) + 1u).ValueOrDie());
    buffer->as_vector()[0] = 0;
    base::span(buffer->as_vector()).subspan(1u).copy_from(report_bytes);
  }

  connection->ProcessInputReport(buffer, buffer->size());
}

void HidConnectionMac::GetFeatureReportAsync(uint8_t report_id,
                                             ReadCallback callback) {
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_feature_report_size() + 1);
  CFIndex report_size = buffer->size();

  // The IOHIDDevice object is shared with the UI thread and so this function
  // should probably be called there but it may block and the asynchronous
  // version is NOT IMPLEMENTED. I've examined the open source implementation
  // of this function and believe it is a simple enough wrapper around the
  // kernel API that this is safe.
  IOReturn result =
      IOHIDDeviceGetReport(device_.get(), kIOHIDReportTypeFeature, report_id,
                           buffer->as_vector().data(), &report_size);
  if (result == kIOReturnSuccess) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HidConnectionMac::ReturnAsyncResult, this,
                                  base::BindOnce(std::move(callback), true,
                                                 buffer, report_size)));
  } else {
    HID_LOG(EVENT) << "Failed to get feature report: " << HexErrorCode(result);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HidConnectionMac::ReturnAsyncResult, this,
                       base::BindOnce(std::move(callback), false, nullptr, 0)));
  }
}

void HidConnectionMac::SetReportAsync(
    IOHIDReportType report_type,
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  auto data = base::span(buffer->as_vector());
  uint8_t report_id = data[0u];
  if (report_id == 0) {
    // OS X only expects the first byte of the buffer to be the report ID if the
    // report ID is non-zero.
    data = data.subspan(1u);
  }

  // The IOHIDDevice object is shared with the UI thread and so this function
  // should probably be called there but it may block and the asynchronous
  // version is NOT IMPLEMENTED. I've examined the open source implementation
  // of this function and believe it is a simple enough wrapper around the
  // kernel API that this is safe.
  IOReturn result = IOHIDDeviceSetReport(device_.get(), report_type, report_id,
                                         data.data(), data.size());
  if (result == kIOReturnSuccess) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HidConnectionMac::ReturnAsyncResult, this,
                                  base::BindOnce(std::move(callback), true)));
  } else {
    HID_LOG(EVENT) << "Failed to set report: " << HexErrorCode(result);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HidConnectionMac::ReturnAsyncResult, this,
                                  base::BindOnce(std::move(callback), false)));
  }
}

void HidConnectionMac::ReturnAsyncResult(base::OnceClosure callback) {
  // This function is used so that the last reference to |this| can be released
  // on the thread where it was created.
  std::move(callback).Run();
}

}  // namespace device
