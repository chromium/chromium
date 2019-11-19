// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_

#include <memory>
#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

// Linux reports breaks and parity errors by inserting the sequence '\377\0x'
// into the byte stream where 'x' is '\0' for a break and the corrupted byte for
// a parity error.
enum class ErrorDetectState { NO_ERROR, MARK_377_SEEN, MARK_0_SEEN };

class SerialIoHandlerPosix : public SerialIoHandler {
 protected:
  // SerialIoHandler impl.
  void ReadImpl() override;
  void WriteImpl() override;
  void CancelReadImpl() override;
  void CancelWriteImpl() override;
  bool ConfigurePortImpl() override;
  bool PostOpen() override;
  bool Flush() const override;
  mojom::SerialPortControlSignalsPtr GetControlSignals() const override;
  bool SetControlSignals(
      const mojom::SerialHostControlSignals& control_signals) override;
  mojom::SerialConnectionInfoPtr GetPortInfo() const override;
  int CheckReceiveError(char* buffer,
                        int buffer_len,
                        int bytes_read,
                        bool& break_detected,
                        bool& parity_error_detected);

 private:
  friend class SerialIoHandler;
  friend class SerialIoHandlerPosixTest;

  SerialIoHandlerPosix(
      const base::FilePath& port,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~SerialIoHandlerPosix() override;

  void AttemptRead(bool within_read);
  void RunReadCompleted(bool within_read,
                        int bytes_read,
                        mojom::SerialReceiveError error);

  // Called when file() is writable without blocking.
  void OnFileCanWriteWithoutBlocking();

  void EnsureWatchingReads();
  void EnsureWatchingWrites();

  std::unique_ptr<base::FileDescriptorWatcher::Controller> file_read_watcher_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> file_write_watcher_;

  ErrorDetectState error_detect_state_;
  bool parity_check_enabled_;
  char chars_stashed_[2];
  int num_chars_stashed_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandlerPosix);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
