// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/hid/hid_connection_linux.h"

#include <errno.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/hid/hid_service.h"

// These are already defined in newer versions of linux/hidraw.h.
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#endif
#ifndef HIDIOCGFEATURE
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

namespace device {

class HidConnectionLinux::BlockingTaskRunnerHelper {
 public:
  BlockingTaskRunnerHelper(base::ScopedFD fd,
                           scoped_refptr<HidDeviceInfo> device_info,
                           base::WeakPtr<HidConnectionLinux> connection,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : fd_(std::move(fd)),
        connection_(connection),
        origin_task_runner_(std::move(task_runner)) {
    // Report buffers must always have room for the report ID.
    report_buffer_size_ = device_info->max_input_report_size() + 1;
    has_report_id_ = device_info->has_report_id();

    // Starts the FileDescriptorWatcher that reads input events from the device.
    // Must be called on a thread that has a base::MessageLoopForIO.
    file_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        fd_.get(), base::BindRepeating(
                       &BlockingTaskRunnerHelper::OnFileCanReadWithoutBlocking,
                       base::Unretained(this)));
  }

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  bool Write(scoped_refptr<base::RefCountedBytes> buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    ssize_t result =
        HANDLE_EINTR(write(fd_.get(), buffer->data(), buffer->size()));
    if (result < 0) {
      HID_PLOG(EVENT) << "Write failed";
      return false;
    } else {
      if (static_cast<size_t>(result) != buffer->size()) {
        HID_LOG(EVENT) << "Incomplete HID write: " << result
                       << " != " << buffer->size();
      }
      return true;
    }
  }

  std::tuple<bool, scoped_refptr<base::RefCountedBytes>, int> GetFeatureReport(
      uint8_t report_id,
      scoped_refptr<base::RefCountedBytes> buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    int result = HANDLE_EINTR(
        ioctl(fd_.get(), HIDIOCGFEATURE(buffer->size()), buffer->data()));
    if (result < 0) {
      HID_PLOG(EVENT) << "Failed to get feature report";
      return std::make_tuple(false, nullptr, 0);
    } else if (result == 0) {
      HID_LOG(EVENT) << "Get feature result too short.";
      return std::make_tuple(false, nullptr, 0);
    } else if (report_id == 0) {
      // Linux adds a 0 to the beginning of the data received from the device.
      auto copied_buffer =
          base::MakeRefCounted<base::RefCountedBytes>(result - 1);
      memcpy(copied_buffer->as_vector().data(), buffer->data() + 1, result - 1);
      return std::make_tuple(true, std::move(copied_buffer), result - 1);
    } else {
      return std::make_tuple(true, std::move(buffer), result);
    }
  }

  bool SendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    int result = HANDLE_EINTR(
        ioctl(fd_.get(), HIDIOCSFEATURE(buffer->size()), buffer->data()));
    if (result < 0) {
      HID_PLOG(EVENT) << "Failed to send feature report";
      return false;
    } else {
      return true;
    }
  }

 private:
  void OnFileCanReadWithoutBlocking() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto buffer =
        base::MakeRefCounted<base::RefCountedBytes>(report_buffer_size_);
    uint8_t* data = buffer->as_vector().data();
    size_t length = report_buffer_size_;
    if (!has_report_id_) {
      // Linux will not prefix the buffer with a report ID if report IDs are not
      // used by the device. Prefix the buffer with 0.
      *data++ = 0;
      length--;
    }

    ssize_t bytes_read = HANDLE_EINTR(read(fd_.get(), data, length));
    if (bytes_read < 0) {
      if (errno != EAGAIN) {
        HID_PLOG(EVENT) << "Read failed";
        // This assumes that the error is unrecoverable and disables reading
        // from the device until it has been re-opened.
        // TODO(reillyg): Investigate starting and stopping the file descriptor
        // watcher in response to pending read requests so that per-request
        // errors can be returned to the client.
        file_watcher_.reset();
      }
      return;
    }
    if (!has_report_id_) {
      // Behave as if the byte prefixed above as the the report ID was read.
      bytes_read++;
    }

    origin_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HidConnectionLinux::ProcessInputReport,
                                  connection_, buffer, bytes_read));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::ScopedFD fd_;
  size_t report_buffer_size_;
  bool has_report_id_;
  base::WeakPtr<HidConnectionLinux> connection_;
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> file_watcher_;
};

HidConnectionLinux::HidConnectionLinux(
    scoped_refptr<HidDeviceInfo> device_info,
    base::ScopedFD fd,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    bool allow_protected_reports,
    bool allow_fido_reports)
    : HidConnection(device_info, allow_protected_reports, allow_fido_reports) {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      std::move(blocking_task_runner), std::move(fd), device_info,
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

HidConnectionLinux::~HidConnectionLinux() {}

void HidConnectionLinux::PlatformClose() {
  // By closing the device on the blocking task runner 1) the requirement that
  // base::ScopedFD is destroyed on a thread where I/O is allowed is satisfied
  // and 2) any tasks posted to this task runner that refer to this file will
  // complete before it is closed.
  helper_.Reset();
}

void HidConnectionLinux::PlatformWrite(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::Write)
      .WithArgs(std::move(buffer))
      .Then(std::move(callback));
}

void HidConnectionLinux::PlatformGetFeatureReport(uint8_t report_id,
                                                  ReadCallback callback) {
  // The first byte of the destination buffer is the report ID being requested
  // and is overwritten by the feature report.
  DCHECK_GT(device_info()->max_feature_report_size(), 0u);
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      device_info()->max_feature_report_size() + 1);
  buffer->as_vector().data()[0] = report_id;

  auto callback_wrapper = base::BindOnce(
      [](ReadCallback callback,
         std::tuple<bool, scoped_refptr<base::RefCountedBytes>, int> result) {
        std::move(callback).Run(std::get<0>(result), std::get<1>(result),
                                std::get<2>(result));
      },
      std::move(callback));

  helper_.AsyncCall(&BlockingTaskRunnerHelper::GetFeatureReport)
      .WithArgs(report_id, std::move(buffer))
      .Then(std::move(callback_wrapper));
}

void HidConnectionLinux::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  helper_.AsyncCall(&BlockingTaskRunnerHelper::SendFeatureReport)
      .WithArgs(std::move(buffer))
      .Then(std::move(callback));
}

}  // namespace device
