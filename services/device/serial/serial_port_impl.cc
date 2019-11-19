// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/serial/buffer.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

// static
void SerialPortImpl::Create(
    const base::FilePath& path,
    mojo::PendingReceiver<mojom::SerialPort> receiver,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  // This SerialPortImpl is owned by |receiver| and |watcher|.
  new SerialPortImpl(path, std::move(receiver), std::move(watcher),
                     std::move(ui_task_runner));
}

SerialPortImpl::SerialPortImpl(
    const base::FilePath& path,
    mojo::PendingReceiver<mojom::SerialPort> receiver,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : receiver_(this, std::move(receiver)),
      io_handler_(device::SerialIoHandler::Create(path, ui_task_runner)),
      watcher_(std::move(watcher)),
      in_stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      out_stream_watcher_(FROM_HERE,
                          mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  receiver_.set_disconnect_handler(base::BindOnce(
      [](SerialPortImpl* self) { delete self; }, base::Unretained(this)));
  if (watcher_.is_bound()) {
    watcher_.set_disconnect_handler(base::BindOnce(
        [](SerialPortImpl* self) { delete self; }, base::Unretained(this)));
  }
}

SerialPortImpl::~SerialPortImpl() {
  // Cancel I/O operations so that |io_handler_| drops its self-reference.
  io_handler_->Close(base::DoNothing());
}

void SerialPortImpl::Open(mojom::SerialConnectionOptionsPtr options,
                          mojo::ScopedDataPipeConsumerHandle in_stream,
                          mojo::ScopedDataPipeProducerHandle out_stream,
                          mojo::PendingRemote<mojom::SerialPortClient> client,
                          OpenCallback callback) {
  DCHECK(in_stream);
  DCHECK(out_stream);
  in_stream_ = std::move(in_stream);
  out_stream_ = std::move(out_stream);
  if (client)
    client_.Bind(std::move(client));
  io_handler_->Open(*options, base::BindOnce(&SerialPortImpl::OnOpenCompleted,
                                             weak_factory_.GetWeakPtr(),
                                             std::move(callback)));
}

void SerialPortImpl::ClearSendError(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  // Make sure |io_handler_| is still open and the |in_stream_| has been
  // closed.
  if (!io_handler_ || in_stream_) {
    return;
  }
  in_stream_watcher_.Cancel();
  in_stream_.swap(consumer);
  in_stream_watcher_.Watch(
      in_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialPortImpl::WriteToPort,
                          weak_factory_.GetWeakPtr()));
  in_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::ClearReadError(
    mojo::ScopedDataPipeProducerHandle producer) {
  // Make sure |io_handler_| is still open and the |out_stream_| has been
  // closed.
  if (!io_handler_ || out_stream_) {
    return;
  }
  out_stream_watcher_.Cancel();
  out_stream_.swap(producer);
  out_stream_watcher_.Watch(
      out_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialPortImpl::ReadFromPortAndWriteOut,
                          weak_factory_.GetWeakPtr()));
  out_stream_watcher_.ArmOrNotify();
}

void SerialPortImpl::Flush(FlushCallback callback) {
  std::move(callback).Run(io_handler_->Flush());
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

void SerialPortImpl::Close(CloseCallback callback) {
  io_handler_->Close(std::move(callback));
}

void SerialPortImpl::OnOpenCompleted(OpenCallback callback, bool success) {
  if (success) {
    in_stream_watcher_.Watch(
        in_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&SerialPortImpl::WriteToPort,
                            weak_factory_.GetWeakPtr()));
    in_stream_watcher_.ArmOrNotify();

    out_stream_watcher_.Watch(
        out_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&SerialPortImpl::ReadFromPortAndWriteOut,
                            weak_factory_.GetWeakPtr()));
    out_stream_watcher_.ArmOrNotify();
  }
  std::move(callback).Run(success);
}

void SerialPortImpl::WriteToPort(MojoResult result,
                                 const mojo::HandleSignalsState& state) {
  const void* buffer;
  uint32_t num_bytes;

  if (result == MOJO_RESULT_OK) {
    DCHECK(in_stream_);
    result = in_stream_->BeginReadData(&buffer, &num_bytes,
                                       MOJO_WRITE_DATA_FLAG_NONE);
  }
  if (result == MOJO_RESULT_OK) {
    io_handler_->Write(std::make_unique<SendBuffer>(
        static_cast<const uint8_t*>(buffer), num_bytes,
        base::BindOnce(&SerialPortImpl::OnWriteToPortCompleted,
                       weak_factory_.GetWeakPtr(), num_bytes)));
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
    return;
  }
  // The code should not reach other cases.
  NOTREACHED();
}

void SerialPortImpl::OnWriteToPortCompleted(uint32_t bytes_expected,
                                            uint32_t bytes_sent,
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
  void* buffer;
  uint32_t num_bytes;
  if (result == MOJO_RESULT_OK) {
    DCHECK(out_stream_);
    result = out_stream_->BeginWriteData(&buffer, &num_bytes,
                                         MOJO_WRITE_DATA_FLAG_NONE);
  }
  if (result == MOJO_RESULT_OK) {
    io_handler_->Read(std::make_unique<ReceiveBuffer>(
        static_cast<char*>(buffer), num_bytes,
        base::BindOnce(&SerialPortImpl::WriteToOutStream,
                       weak_factory_.GetWeakPtr())));
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
  NOTREACHED();
}

void SerialPortImpl::WriteToOutStream(uint32_t bytes_read,
                                      mojom::SerialReceiveError error) {
  DCHECK(out_stream_);
  out_stream_->EndWriteData(bytes_read);

  if (error != mojom::SerialReceiveError::NONE) {
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    if (client_) {
      client_->OnReadError(error);
    }
    return;
  }
  out_stream_watcher_.ArmOrNotify();
}

}  // namespace device
