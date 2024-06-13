// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

// static
void SerialPortImpl::Open(
    const base::FilePath& path,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    OpenCallback callback) {
  // This SerialPortImpl is owned by |receiver_| and |watcher_| and will
  // self-destruct on close.
  auto* port = new SerialPortImpl(
      device::SerialIoHandler::Create(path, std::move(ui_task_runner)),
      std::move(client), std::move(watcher));
  port->OpenPort(*options, std::move(callback));
}

// static
void SerialPortImpl::OpenForTesting(
    scoped_refptr<SerialIoHandler> io_handler,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    OpenCallback callback) {
  // This SerialPortImpl is owned by |receiver| and |watcher| and will
  // self-destruct on close.
  auto* port = new SerialPortImpl(std::move(io_handler), std::move(client),
                                  std::move(watcher));
  port->OpenPort(*options, std::move(callback));
}

SerialPortImpl::SerialPortImpl(
    scoped_refptr<SerialIoHandler> io_handler,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher)
    : io_handler_(std::move(io_handler)),
      client_(std::move(client)),
      watcher_(std::move(watcher)),
      in_stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      out_stream_watcher_(FROM_HERE,
                          mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  if (watcher_.is_bound()) {
    watcher_.set_disconnect_handler(base::BindOnce(
        [](SerialPortImpl* self) { delete self; }, base::Unretained(this)));
  }
}

SerialPortImpl::~SerialPortImpl() {
  // Cancel I/O operations so that |io_handler_| drops its self-reference.
  io_handler_->Close(base::DoNothing());
}

void SerialPortImpl::OpenPort(const mojom::SerialConnectionOptions& options,
                              OpenCallback callback) {
  io_handler_->Open(
      options, base::BindOnce(&SerialPortImpl::PortOpened,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SerialPortImpl::PortOpened(OpenCallback callback, bool success) {
  mojo::PendingRemote<SerialPort> port;
  if (success) {
    port = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        [](SerialPortImpl* self) { delete self; }, base::Unretained(this)));
  }

  std::move(callback).Run(std::move(port));

  if (!success)
    delete this;
}

void SerialPortImpl::StartWriting(mojo::ScopedDataPipeConsumerHandle consumer) {
  if (in_stream_) {
    receiver_.ReportBadMessage("Data pipe consumer still open.");
    return;
  }

  in_stream_watcher_.Cancel();
  in_stream_ = std::move(consumer);
  in_stream_watcher_.Watch(
      in_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialPortImpl::WriteToPort,
                          weak_factory_.GetWeakPtr()));
  in_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::StartReading(mojo::ScopedDataPipeProducerHandle producer) {
  if (out_stream_) {
    receiver_.ReportBadMessage("Data pipe producer still open.");
    return;
  }

  out_stream_watcher_.Cancel();
  out_stream_ = std::move(producer);
  out_stream_watcher_.Watch(
      out_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialPortImpl::ReadFromPortAndWriteOut,
                          weak_factory_.GetWeakPtr()));
  out_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::Flush(mojom::SerialPortFlushMode mode,
                           FlushCallback callback) {
  switch (mode) {
    case mojom::SerialPortFlushMode::kReceiveAndTransmit:
      // Do nothing. This case exists to support the chrome.serial.flush()
      // method.
      break;
    case mojom::SerialPortFlushMode::kReceive:
      io_handler_->CancelRead(mojom::SerialReceiveError::NONE);
      break;
    case mojom::SerialPortFlushMode::kTransmit:
      io_handler_->CancelWrite(mojom::SerialSendError::NONE);
      break;
  }

  io_handler_->Flush(mode);

  switch (mode) {
    case mojom::SerialPortFlushMode::kReceiveAndTransmit:
      // Do nothing. This case exists to support the chrome.serial.flush()
      // method.
      break;
    case mojom::SerialPortFlushMode::kReceive:
      if (io_handler_->IsReadPending()) {
        // Delay closing |out_stream_| because |io_handler_| still holds a
        // pointer into the shared memory owned by the pipe.
        read_flush_callback_ = std::move(callback);
        return;
      }

      out_stream_watcher_.Cancel();
      out_stream_.reset();
      break;
    case mojom::SerialPortFlushMode::kTransmit:
      if (io_handler_->IsWritePending()) {
        // Delay closing |in_stream_| because |io_handler_| still holds a
        // pointer into the shared memory owned by the pipe.
        write_flush_callback_ = std::move(callback);
        return;
      }

      in_stream_watcher_.Cancel();
      in_stream_.reset();
      break;
  }

  std::move(callback).Run();
}

void SerialPortImpl::Drain(DrainCallback callback) {
  if (!in_stream_) {
    std::move(callback).Run();
    return;
  }

  drain_callback_ = std::move(callback);
}

void SerialPortImpl::GetControlSignals(GetControlSignalsCallback callback) {
  std::move(callback).Run(io_handler_->GetControlSignals());
}

void SerialPortImpl::SetControlSignals(
    mojom::SerialHostControlSignalsPtr signals,
    SetControlSignalsCallback callback) {
  std::move(callback).Run(io_handler_->SetControlSignals(*signals));
}

void SerialPortImpl::ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                                   ConfigurePortCallback callback) {
  std::move(callback).Run(io_handler_->ConfigurePort(*options));
  // Cancel pending reading as the new configure options are applied.
  io_handler_->CancelRead(mojom::SerialReceiveError::NONE);
}

void SerialPortImpl::GetPortInfo(GetPortInfoCallback callback) {
  std::move(callback).Run(io_handler_->GetPortInfo());
}

void SerialPortImpl::Close(bool flush, CloseCallback callback) {
  if (flush) {
    io_handler_->Flush(mojom::SerialPortFlushMode::kReceiveAndTransmit);
  }

  io_handler_->Close(base::BindOnce(&SerialPortImpl::PortClosed,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void SerialPortImpl::WriteToPort(MojoResult result,
                                 const mojo::HandleSignalsState& state) {
  base::span<const uint8_t> buffer;

  if (result == MOJO_RESULT_OK) {
    DCHECK(in_stream_);
    result = in_stream_->BeginReadData(MOJO_WRITE_DATA_FLAG_NONE, buffer);
  }
  if (result == MOJO_RESULT_OK) {
    io_handler_->Write(buffer,
                       base::BindOnce(&SerialPortImpl::OnWriteToPortCompleted,
                                      weak_factory_.GetWeakPtr()));
    return;
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // If there is no space to write, wait for more space.
    in_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION ||
      result == MOJO_RESULT_CANCELLED) {
    // The |in_stream_| has been closed.
    in_stream_watcher_.Cancel();
    in_stream_.reset();

    if (drain_callback_) {
      io_handler_->Drain();
      std::move(drain_callback_).Run();
    }
    return;
  }
  // The code should not reach other cases.
  NOTREACHED_IN_MIGRATION();
}

void SerialPortImpl::OnWriteToPortCompleted(uint32_t bytes_sent,
                                            mojom::SerialSendError error) {
  DCHECK(in_stream_);
  in_stream_->EndReadData(bytes_sent);

  if (error != mojom::SerialSendError::NONE) {
    in_stream_watcher_.Cancel();
    in_stream_.reset();
    if (client_) {
      client_->OnSendError(error);
    }
    return;
  }

  in_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::ReadFromPortAndWriteOut(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  base::span<uint8_t> buffer;
  if (result == MOJO_RESULT_OK) {
    DCHECK(out_stream_);
    result =
        out_stream_->BeginWriteData(mojo::DataPipeProducerHandle::kNoSizeHint,
                                    MOJO_WRITE_DATA_FLAG_NONE, buffer);
  }
  if (result == MOJO_RESULT_OK) {
    io_handler_->Read(buffer, base::BindOnce(&SerialPortImpl::WriteToOutStream,
                                             weak_factory_.GetWeakPtr()));
    return;
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // If there is no space to write, wait for more space.
    out_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION ||
      result == MOJO_RESULT_CANCELLED) {
    // The |out_stream_| has been closed.
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    return;
  }
  // The code should not reach other cases.
  NOTREACHED_IN_MIGRATION() << "Unexpected Mojo result: " << result;
}

void SerialPortImpl::WriteToOutStream(uint32_t bytes_read,
                                      mojom::SerialReceiveError error) {
  DCHECK(out_stream_);
  out_stream_->EndWriteData(bytes_read);

  if (error != mojom::SerialReceiveError::NONE) {
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    if (client_)
      client_->OnReadError(error);
    if (read_flush_callback_)
      std::move(read_flush_callback_).Run();
    return;
  }

  if (read_flush_callback_) {
    std::move(read_flush_callback_).Run();
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    return;
  }

  out_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::PortClosed(CloseCallback callback) {
  std::move(callback).Run();
  delete this;
}

}  // namespace device
