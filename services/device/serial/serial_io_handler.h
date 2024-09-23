// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

// Provides a simplified interface for performing asynchronous I/O on serial
// devices by hiding platform-specific MessageLoop interfaces. Pending I/O
// operations hold a reference to this object until completion so that memory
// doesn't disappear out from under the OS.
class SerialIoHandler : public base::RefCountedThreadSafe<SerialIoHandler> {
 public:
  // Constructs an instance of some platform-specific subclass.
  static scoped_refptr<SerialIoHandler> Create(
      const base::FilePath& port,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  SerialIoHandler(const SerialIoHandler&) = delete;
  SerialIoHandler& operator=(const SerialIoHandler&) = delete;

  using OpenCompleteCallback = base::OnceCallback<void(bool success)>;
  using ReadCompleteCallback =
      base::OnceCallback<void(uint32_t bytes_read, mojom::SerialReceiveError)>;
  using WriteCompleteCallback =
      base::OnceCallback<void(uint32_t bytes_written, mojom::SerialSendError)>;

  // Initiates an asynchronous Open of the device.
  virtual void Open(const mojom::SerialConnectionOptions& options,
                    OpenCompleteCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Signals that the port has been opened.
  void OnPathOpened(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      base::ScopedFD fd);

  // Signals that the permission broker failed to open the port.
  void OnPathOpenError(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      const std::string& error_name,
      const std::string& error_message);

  // Reports the open error from the permission broker.
  void ReportPathOpenError(const std::string& error_name,
                           const std::string& error_message);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Performs an async read operation. Behavior is undefined if this is called
  // while a read is already pending. Otherwise, |callback| will be called
  // (potentially synchronously) with a result. |buffer| must remain valid until
  // |callback| is run.
  void Read(base::span<uint8_t> buffer, ReadCompleteCallback callback);

  // Performs an async write operation. Behavior is undefined if this is called
  // while a write is already pending. Otherwise, |callback| will be called
  // (potentially synchronously) with a result. |buffer| must remain valid until
  // |callback| is run.
  void Write(base::span<const uint8_t> buffer, WriteCompleteCallback callback);

  // Indicates whether or not a read is currently pending.
  bool IsReadPending() const;

  // Indicates whether or not a write is currently pending.
  bool IsWritePending() const;

  // Attempts to cancel a pending read operation.
  void CancelRead(mojom::SerialReceiveError reason);

  // Attempts to cancel a pending write operation.
  void CancelWrite(mojom::SerialSendError reason);

  // Flushes input and output buffers.
  virtual void Flush(mojom::SerialPortFlushMode mode) const = 0;

  // Drains output buffers.
  virtual void Drain() = 0;

  // Reads current control signals (DCD, CTS, etc.) into an existing
  // DeviceControlSignals structure. Returns |true| iff the signals were
  // successfully read.
  virtual mojom::SerialPortControlSignalsPtr GetControlSignals() const = 0;

  // Sets one or more control signals. Returns |true| iff the signals were
  // successfully set. Flags not present in |control_signals| are unchanged.
  virtual bool SetControlSignals(
      const mojom::SerialHostControlSignals& control_signals) = 0;

  // Performs platform-specific port configuration. Returns |true| iff
  // configuration was successful.
  bool ConfigurePort(const mojom::SerialConnectionOptions& options);

  // Performs a platform-specific port configuration query. Fills values in an
  // existing ConnectionInfo. Returns |true| iff port configuration was
  // successfully retrieved.
  virtual mojom::SerialConnectionInfoPtr GetPortInfo() const = 0;

  // Initiates an asynchronous close of the port.
  void Close(base::OnceClosure callback);

 protected:
  explicit SerialIoHandler(
      const base::FilePath& port,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  virtual ~SerialIoHandler();

  // Performs a platform-specific read operation. This must guarantee that
  // ReadCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of ReadImpl may call ReadCompleted directly.
  virtual void ReadImpl() = 0;

  // Performs a platform-specific write operation. This must guarantee that
  // WriteCompleted is called when the underlying async operation is completed
  // or the SerialIoHandler instance will leak.
  // NOTE: Implementations of WriteImpl may call WriteCompleted directly.
  virtual void WriteImpl() = 0;

  // Platform-specific read cancelation.
  virtual void CancelReadImpl() = 0;

  // Platform-specific write cancelation.
  virtual void CancelWriteImpl() = 0;

  // Platform-specific port configuration applies options_ to the device.
  virtual bool ConfigurePortImpl() = 0;

  // Performs platform-specific, one-time port configuration on open.
  virtual bool PostOpen();

  // Performs platform-specific operations before |file_| is closed.
  virtual void PreClose();

  // Called by the implementation to signal that the active read has completed.
  // WARNING: Calling this method can destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void ReadCompleted(int bytes_read, mojom::SerialReceiveError error);

  // Called by the implementation to signal that the active write has completed.
  // WARNING: Calling this method may destroy the SerialIoHandler instance
  // if the associated I/O operation was the only thing keeping it alive.
  void WriteCompleted(int bytes_written, mojom::SerialSendError error);

  const base::File& file() const { return file_; }

  base::span<uint8_t> pending_read_buffer() const {
    return pending_read_buffer_;
  }

  mojom::SerialReceiveError read_cancel_reason() const {
    return read_cancel_reason_;
  }

  bool read_canceled() const { return read_canceled_; }

  base::span<const uint8_t> pending_write_buffer() const {
    return pending_write_buffer_;
  }

  mojom::SerialSendError write_cancel_reason() const {
    return write_cancel_reason_;
  }

  bool write_canceled() const { return write_canceled_; }

  const mojom::SerialConnectionOptions& options() const { return options_; }

  base::SingleThreadTaskRunner* ui_thread_task_runner() const {
    return ui_thread_task_runner_.get();
  }

  const base::FilePath& port() const { return port_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  friend class base::RefCountedThreadSafe<SerialIoHandler>;

  void MergeConnectionOptions(const mojom::SerialConnectionOptions& options);

  // Continues an Open operation on the FILE thread.
  void StartOpen(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Finalizes an Open operation (continued from StartOpen) on the IO thread.
  void FinishOpen(base::File file);

  // Continues a Close operation on the FILE thread.
  static void DoClose(base::File port);

  // File for the opened serial device. This value is only modified from the IO
  // thread.
  base::File file_;

  // Currently applied connection options.
  mojom::SerialConnectionOptions options_;

  base::raw_span<uint8_t> pending_read_buffer_;
  ReadCompleteCallback pending_read_callback_;
  mojom::SerialReceiveError read_cancel_reason_;
  bool read_canceled_;

  base::raw_span<const uint8_t> pending_write_buffer_;
  WriteCompleteCallback pending_write_callback_;
  mojom::SerialSendError write_cancel_reason_;
  bool write_canceled_;

  // Callback to handle the completion of a pending Open() request.
  OpenCompleteCallback open_complete_;

  const base::FilePath port_;

  // On Chrome OS, PermissionBrokerClient should be called on the UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_H_
