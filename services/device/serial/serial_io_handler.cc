// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_io_handler.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

SerialIoHandler::SerialIoHandler(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : port_(port), ui_thread_task_runner_(ui_thread_task_runner) {
  options_.bitrate = 9600;
  options_.data_bits = mojom::SerialDataBits::EIGHT;
  options_.parity_bit = mojom::SerialParityBit::NO_PARITY;
  options_.stop_bits = mojom::SerialStopBits::ONE;
  options_.cts_flow_control = false;
  options_.has_cts_flow_control = true;
}

SerialIoHandler::~SerialIoHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Close(base::DoNothing());
}

void SerialIoHandler::Open(const mojom::SerialConnectionOptions& options,
                           OpenCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!open_complete_);
  DCHECK(!port_.empty());
  open_complete_ = std::move(callback);
  DCHECK(ui_thread_task_runner_.get());
  MergeConnectionOptions(options);

#if BUILDFLAG(IS_CHROMEOS)
  // Note: dbus clients are destroyed in PostDestroyThreads so passing |client|
  // as unretained is safe.
  auto* client = chromeos::PermissionBrokerClient::Get();
  DCHECK(client) << "Could not get permission_broker client.";
  // PermissionBrokerClient should be called on the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&chromeos::PermissionBrokerClient::OpenPath,
                     base::Unretained(client), port_.value(),
                     base::BindRepeating(&SerialIoHandler::OnPathOpened, this,
                                         task_runner),
                     base::BindRepeating(&SerialIoHandler::OnPathOpenError,
                                         this, task_runner)));
#else
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SerialIoHandler::StartOpen, this,
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)

void SerialIoHandler::OnPathOpened(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    base::ScopedFD fd) {
  base::File file(std::move(fd));
  io_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SerialIoHandler::FinishOpen, this, std::move(file)));
}

void SerialIoHandler::OnPathOpenError(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    const std::string& error_name,
    const std::string& error_message) {
  io_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SerialIoHandler::ReportPathOpenError, this,
                                error_name, error_message));
}

void SerialIoHandler::ReportPathOpenError(const std::string& error_name,
                                          const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(open_complete_);
  SERIAL_LOG(ERROR) << "Permission broker failed to open '" << port_
                    << "': " << error_name << ": " << error_message;
  std::move(open_complete_).Run(false);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void SerialIoHandler::MergeConnectionOptions(
    const mojom::SerialConnectionOptions& options) {
  if (options.bitrate) {
    options_.bitrate = options.bitrate;
  }
  if (options.data_bits != mojom::SerialDataBits::NONE) {
    options_.data_bits = options.data_bits;
  }
  if (options.parity_bit != mojom::SerialParityBit::NONE) {
    options_.parity_bit = options.parity_bit;
  }
  if (options.stop_bits != mojom::SerialStopBits::NONE) {
    options_.stop_bits = options.stop_bits;
  }
  if (options.has_cts_flow_control) {
    DCHECK(options_.has_cts_flow_control);
    options_.cts_flow_control = options.cts_flow_control;
  }
}

void SerialIoHandler::StartOpen(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  DCHECK(open_complete_);
  DCHECK(!file_.IsValid());
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WIN_EXCLUSIVE_READ | base::File::FLAG_WRITE |
              base::File::FLAG_WIN_EXCLUSIVE_WRITE | base::File::FLAG_ASYNC |
              base::File::FLAG_TERMINAL_DEVICE;
  base::File file(port_, flags);
  io_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&SerialIoHandler::FinishOpen, this, std::move(file)));
}

void SerialIoHandler::FinishOpen(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(open_complete_);
  if (!file.IsValid()) {
    SERIAL_LOG(ERROR) << "Failed to open serial port: "
                      << base::File::ErrorToString(file.error_details());
    std::move(open_complete_).Run(false);
    return;
  }

  file_ = std::move(file);

  bool success = PostOpen() && ConfigurePortImpl();
  if (!success)
    Close(base::DoNothing());

  std::move(open_complete_).Run(success);
}

bool SerialIoHandler::PostOpen() {
  return true;
}

void SerialIoHandler::PreClose() {}

void SerialIoHandler::Close(base::OnceClosure callback) {
  if (file_.IsValid()) {
    CancelRead(mojom::SerialReceiveError::DISCONNECTED);
    CancelWrite(mojom::SerialSendError::DISCONNECTED);
    PreClose();
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&SerialIoHandler::DoClose, std::move(file_)),
        std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

// static
void SerialIoHandler::DoClose(base::File port) {
  // port closed by destructor.
}

void SerialIoHandler::Read(base::span<uint8_t> buffer,
                           ReadCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsReadPending());

  pending_read_buffer_ = buffer;
  pending_read_callback_ = std::move(callback);
  read_canceled_ = false;
  AddRef();

  if (!file().IsValid()) {
    ReadCompleted(0, mojom::SerialReceiveError::DISCONNECTED);
    return;
  }

  ReadImpl();
}

void SerialIoHandler::Write(base::span<const uint8_t> buffer,
                            WriteCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsWritePending());

  pending_write_buffer_ = buffer;
  pending_write_callback_ = std::move(callback);
  write_canceled_ = false;
  AddRef();

  if (!file().IsValid()) {
    WriteCompleted(0, mojom::SerialSendError::DISCONNECTED);
    return;
  }

  WriteImpl();
}

void SerialIoHandler::ReadCompleted(int bytes_read,
                                    mojom::SerialReceiveError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsReadPending());
  pending_read_buffer_ = base::span<uint8_t>();
  std::move(pending_read_callback_).Run(bytes_read, error);
  Release();
}

void SerialIoHandler::WriteCompleted(int bytes_written,
                                     mojom::SerialSendError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsWritePending());
  pending_write_buffer_ = base::span<const uint8_t>();
  std::move(pending_write_callback_).Run(bytes_written, error);
  Release();
}

bool SerialIoHandler::IsReadPending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !pending_read_callback_.is_null();
}

bool SerialIoHandler::IsWritePending() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !pending_write_callback_.is_null();
}

void SerialIoHandler::CancelRead(mojom::SerialReceiveError reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsReadPending() && !read_canceled_) {
    read_canceled_ = true;
    read_cancel_reason_ = reason;
    CancelReadImpl();
  }
}

void SerialIoHandler::CancelWrite(mojom::SerialSendError reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsWritePending() && !write_canceled_) {
    write_canceled_ = true;
    write_cancel_reason_ = reason;
    CancelWriteImpl();
  }
}

bool SerialIoHandler::ConfigurePort(
    const mojom::SerialConnectionOptions& options) {
  MergeConnectionOptions(options);
  return ConfigurePortImpl();
}

}  // namespace device
