// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_connection.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/common/api/serial.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace extensions {

namespace {

const int kDefaultBufferSize = 4096;

api::serial::SendError ConvertSendErrorFromMojo(
    device::mojom::SerialSendError input) {
  switch (input) {
    case device::mojom::SerialSendError::NONE:
      return api::serial::SendError::kNone;
    case device::mojom::SerialSendError::DISCONNECTED:
      return api::serial::SendError::kDisconnected;
    case device::mojom::SerialSendError::SYSTEM_ERROR:
      return api::serial::SendError::kSystemError;
  }
  return api::serial::SendError::kNone;
}

api::serial::ReceiveError ConvertReceiveErrorFromMojo(
    device::mojom::SerialReceiveError input) {
  switch (input) {
    case device::mojom::SerialReceiveError::NONE:
      return api::serial::ReceiveError::kNone;
    case device::mojom::SerialReceiveError::DISCONNECTED:
      return api::serial::ReceiveError::kDisconnected;
    case device::mojom::SerialReceiveError::DEVICE_LOST:
      return api::serial::ReceiveError::kDeviceLost;
    case device::mojom::SerialReceiveError::BREAK:
      return api::serial::ReceiveError::kBreak;
    case device::mojom::SerialReceiveError::FRAME_ERROR:
      return api::serial::ReceiveError::kFrameError;
    case device::mojom::SerialReceiveError::OVERRUN:
      return api::serial::ReceiveError::kOverrun;
    case device::mojom::SerialReceiveError::BUFFER_OVERFLOW:
      return api::serial::ReceiveError::kBufferOverflow;
    case device::mojom::SerialReceiveError::PARITY_ERROR:
      return api::serial::ReceiveError::kParityError;
    case device::mojom::SerialReceiveError::SYSTEM_ERROR:
      return api::serial::ReceiveError::kSystemError;
  }
  return api::serial::ReceiveError::kNone;
}

api::serial::DataBits ConvertDataBitsFromMojo(
    device::mojom::SerialDataBits input) {
  switch (input) {
    case device::mojom::SerialDataBits::NONE:
      return api::serial::DataBits::kNone;
    case device::mojom::SerialDataBits::SEVEN:
      return api::serial::DataBits::kSeven;
    case device::mojom::SerialDataBits::EIGHT:
      return api::serial::DataBits::kEight;
  }
  return api::serial::DataBits::kNone;
}

device::mojom::SerialDataBits ConvertDataBitsToMojo(
    api::serial::DataBits input) {
  switch (input) {
    case api::serial::DataBits::kNone:
      return device::mojom::SerialDataBits::NONE;
    case api::serial::DataBits::kSeven:
      return device::mojom::SerialDataBits::SEVEN;
    case api::serial::DataBits::kEight:
      return device::mojom::SerialDataBits::EIGHT;
  }
  return device::mojom::SerialDataBits::NONE;
}

api::serial::ParityBit ConvertParityBitFromMojo(
    device::mojom::SerialParityBit input) {
  switch (input) {
    case device::mojom::SerialParityBit::NONE:
      return api::serial::ParityBit::kNone;
    case device::mojom::SerialParityBit::ODD:
      return api::serial::ParityBit::kOdd;
    case device::mojom::SerialParityBit::NO_PARITY:
      return api::serial::ParityBit::kNo;
    case device::mojom::SerialParityBit::EVEN:
      return api::serial::ParityBit::kEven;
  }
  return api::serial::ParityBit::kNone;
}

device::mojom::SerialParityBit ConvertParityBitToMojo(
    api::serial::ParityBit input) {
  switch (input) {
    case api::serial::ParityBit::kNone:
      return device::mojom::SerialParityBit::NONE;
    case api::serial::ParityBit::kNo:
      return device::mojom::SerialParityBit::NO_PARITY;
    case api::serial::ParityBit::kOdd:
      return device::mojom::SerialParityBit::ODD;
    case api::serial::ParityBit::kEven:
      return device::mojom::SerialParityBit::EVEN;
  }
  return device::mojom::SerialParityBit::NONE;
}

api::serial::StopBits ConvertStopBitsFromMojo(
    device::mojom::SerialStopBits input) {
  switch (input) {
    case device::mojom::SerialStopBits::NONE:
      return api::serial::StopBits::kNone;
    case device::mojom::SerialStopBits::ONE:
      return api::serial::StopBits::kOne;
    case device::mojom::SerialStopBits::TWO:
      return api::serial::StopBits::kTwo;
  }
  return api::serial::StopBits::kNone;
}

device::mojom::SerialStopBits ConvertStopBitsToMojo(
    api::serial::StopBits input) {
  switch (input) {
    case api::serial::StopBits::kNone:
      return device::mojom::SerialStopBits::NONE;
    case api::serial::StopBits::kOne:
      return device::mojom::SerialStopBits::ONE;
    case api::serial::StopBits::kTwo:
      return device::mojom::SerialStopBits::TWO;
  }
  return device::mojom::SerialStopBits::NONE;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<SerialConnection>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<SerialConnection> >*
ApiResourceManager<SerialConnection>::GetFactoryInstance() {
  return g_factory.Pointer();
}

SerialConnection::SerialConnection(const std::string& owner_extension_id)
    : ApiResource(owner_extension_id),
      persistent_(false),
      buffer_size_(kDefaultBufferSize),
      receive_timeout_(0),
      send_timeout_(0),
      paused_(true),
      read_error_(std::nullopt),
      bytes_written_(0),
      receive_pipe_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
}

SerialConnection::~SerialConnection() = default;

bool SerialConnection::IsPersistent() const {
  return persistent();
}

void SerialConnection::set_buffer_size(int buffer_size) {
  buffer_size_ = buffer_size;
}

void SerialConnection::set_receive_timeout(int receive_timeout) {
  receive_timeout_ = receive_timeout;
}

void SerialConnection::set_send_timeout(int send_timeout) {
  send_timeout_ = send_timeout;
}

void SerialConnection::SetPaused(bool paused) {
  DCHECK(serial_port_);
  if (paused_ == paused)
    return;

  paused_ = paused;
  if (paused_) {
    // Make sure no pending timeout task fires.
    receive_timeout_task_.Cancel();
  } else {
    // If |receive_pipe_| is closed and there is no pending ReceiveError event,
    // try to reconnect the data pipe.
    if (!receive_pipe_ && !read_error_)
      SetUpReceiveDataPipe();
    receive_pipe_watcher_.ArmOrNotify();
    receive_timeout_task_.Cancel();
    SetTimeoutCallback();
  }
}

void SerialConnection::SetConnectionErrorHandler(
    base::OnceClosure connection_error_handler) {
  if (!serial_port_.is_connected()) {
    // Already being disconnected, run client's error handler immediatelly.
    std::move(connection_error_handler).Run();
    return;
  }
  connection_error_handler_ = std::move(connection_error_handler);
}

void SerialConnection::Open(api::SerialPortManager* port_manager,
                            const std::string& path,
                            const api::serial::ConnectionOptions& options,
                            OpenCompleteCallback callback) {
  DCHECK(!serial_port_);
  DCHECK(!send_pipe_);
  DCHECK(!receive_pipe_);

  if (options.persistent)
    set_persistent(*options.persistent);
  if (options.name)
    set_name(*options.name);
  if (options.buffer_size)
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout)
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout)
    set_send_timeout(*options.send_timeout);

  mojo::PendingRemote<device::mojom::SerialPortClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  port_manager->OpenPort(
      path, device::mojom::SerialConnectionOptions::From(options),
      std::move(client),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&SerialConnection::OnOpen, weak_factory_.GetWeakPtr(),
                         std::move(client_receiver), std::move(callback)),
          mojo::NullRemote()));
}

void SerialConnection::CreatePipe(
    mojo::ScopedDataPipeProducerHandle* producer,
    mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = buffer_size_;

  CHECK_EQ(MOJO_RESULT_OK,
           mojo::CreateDataPipe(&options, *producer, *consumer));
}

void SerialConnection::SetUpReceiveDataPipe() {
  mojo::ScopedDataPipeProducerHandle producer;
  CreatePipe(&producer, &receive_pipe_);

  serial_port_->StartReading(std::move(producer));

  receive_pipe_watcher_.Watch(
      receive_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialConnection::OnReadPipeReadableOrClosed,
                          weak_factory_.GetWeakPtr()));
}

void SerialConnection::SetUpSendDataPipe() {
  mojo::ScopedDataPipeConsumerHandle consumer;
  CreatePipe(&send_pipe_, &consumer);

  serial_port_->StartWriting(std::move(consumer));

  send_pipe_watcher_.Watch(
      send_pipe_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&SerialConnection::OnSendPipeWritableOrClosed,
                          weak_factory_.GetWeakPtr()));
}

void SerialConnection::OnReadError(device::mojom::SerialReceiveError error) {
  DCHECK_NE(device::mojom::SerialReceiveError::NONE, error);
  if (receive_pipe_) {
    // Wait for remaining data to be read from the pipe before signaling an
    // error.
    read_error_ = error;
    return;
  }
  // Dispatch OnReceiveError event when the data pipe has been closed.
  receive_event_cb_.Run(std::vector<uint8_t>(),
                        ConvertReceiveErrorFromMojo(error));
}

void SerialConnection::OnSendError(device::mojom::SerialSendError error) {
  DCHECK_NE(device::mojom::SerialSendError::NONE, error);
  send_pipe_watcher_.Cancel();
  send_pipe_.reset();

  // Invoking `send_complete_` may free `this`.
  size_t bytes_written = bytes_written_;
  bytes_written_ = 0;

  if (send_complete_) {
    // Respond the send request with bytes written currently.
    std::move(send_complete_)
        .Run(bytes_written, ConvertSendErrorFromMojo(error));
  }
}

void SerialConnection::OnOpen(
    mojo::PendingReceiver<device::mojom::SerialPortClient> client_receiver,
    OpenCompleteCallback callback,
    mojo::PendingRemote<device::mojom::SerialPort> serial_port) {
  if (!serial_port.is_valid()) {
    std::move(callback).Run(false);
    return;
  }

  serial_port_.Bind(std::move(serial_port));
  serial_port_.set_disconnect_handler(base::BindOnce(
      &SerialConnection::OnConnectionError, base::Unretained(this)));

  SetUpReceiveDataPipe();
  SetUpSendDataPipe();
  client_receiver_.Bind(std::move(client_receiver));
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &SerialConnection::OnClientReceiverClosed, weak_factory_.GetWeakPtr()));
  std::move(callback).Run(true);
}

void SerialConnection::OnReadPipeClosed() {
  receive_pipe_watcher_.Cancel();
  receive_pipe_.reset();

  if (read_error_) {
    // Dispatch OnReceiveError if there is a pending error.
    auto error = ConvertReceiveErrorFromMojo(read_error_.value());
    read_error_.reset();
    receive_event_cb_.Run(std::vector<uint8_t>(), error);
  }
}

void SerialConnection::OnReadPipeReadableOrClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  // Data pipe disconnected.
  if (result != MOJO_RESULT_OK) {
    OnReadPipeClosed();
    return;
  }
  // Return when it is paused, the watcher will be disarmed.
  if (paused_)
    return;

  DCHECK(receive_pipe_);
  base::span<const uint8_t> buffer;
  result = receive_pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_pipe_watcher_.ArmOrNotify();
    return;
  }
  // For remote pipe producer handle has been closed.
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnReadPipeClosed();
    return;
  }
  std::vector<uint8_t> data(buffer.begin(), buffer.end());
  result = receive_pipe_->EndReadData(buffer.size());
  DCHECK_EQ(MOJO_RESULT_OK, result);

  // Reset the timeout timer and arm the watcher in preparation for the next
  // read. This will be undone if |receive_event_cb_| calls SetPaused(true).
  receive_timeout_task_.Cancel();
  SetTimeoutCallback();
  receive_pipe_watcher_.ArmOrNotify();

  receive_event_cb_.Run(std::move(data), api::serial::ReceiveError::kNone);
}

void SerialConnection::StartPolling(const ReceiveEventCallback& callback) {
  receive_event_cb_ = callback;
  DCHECK(receive_event_cb_);
  DCHECK(receive_pipe_);
  DCHECK(paused_);
  SetPaused(false);
}

void SerialConnection::Send(const std::vector<uint8_t>& data,
                            SendCompleteCallback callback) {
  if (send_complete_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0,
                                  api::serial::SendError::kPending));
    return;
  }

  DCHECK(serial_port_);
  bytes_written_ = 0;
  send_complete_ = std::move(callback);

  DCHECK(data_to_send_.empty());
  data_to_send_.assign(data.begin(), data.end());

  if (!send_pipe_)
    SetUpSendDataPipe();
  send_pipe_watcher_.ArmOrNotify();

  send_timeout_task_.Cancel();
  if (send_timeout_ > 0) {
    send_timeout_task_.Reset(base::BindOnce(&SerialConnection::OnSendTimeout,
                                            weak_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, send_timeout_task_.callback(),
        base::Milliseconds(send_timeout_));
  }
}

void SerialConnection::Configure(const api::serial::ConnectionOptions& options,
                                 ConfigureCompleteCallback callback) {
  DCHECK(serial_port_);
  if (options.persistent)
    set_persistent(*options.persistent);
  if (options.name)
    set_name(*options.name);
  if (options.buffer_size)
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout)
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout)
    set_send_timeout(*options.send_timeout);
  serial_port_->ConfigurePort(
      device::mojom::SerialConnectionOptions::From(options),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::GetInfo(GetInfoCompleteCallback callback) const {
  DCHECK(serial_port_);

  auto info = std::make_unique<api::serial::ConnectionInfo>();
  info->paused = paused_;
  info->persistent = persistent_;
  info->name = name_;
  info->buffer_size = buffer_size_;
  info->receive_timeout = receive_timeout_;
  info->send_timeout = send_timeout_;

  auto resp_callback = base::BindOnce(
      [](GetInfoCompleteCallback callback,
         std::unique_ptr<api::serial::ConnectionInfo> info,
         device::mojom::SerialConnectionInfoPtr port_info) {
        if (!port_info) {
          // Even without remote port info, return partial info and indicate
          // that it's not complete info.
          std::move(callback).Run(false, std::move(info));
          return;
        }
        info->bitrate = port_info->bitrate;
        info->data_bits = ConvertDataBitsFromMojo(port_info->data_bits);
        info->parity_bit = ConvertParityBitFromMojo(port_info->parity_bit);
        info->stop_bits = ConvertStopBitsFromMojo(port_info->stop_bits);
        info->cts_flow_control = port_info->cts_flow_control;
        std::move(callback).Run(true, std::move(info));
      },
      std::move(callback), std::move(info));
  serial_port_->GetPortInfo(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(resp_callback), nullptr));
}

void SerialConnection::Flush(device::mojom::SerialPortFlushMode mode,
                             FlushCompleteCallback callback) const {
  DCHECK(serial_port_);
  return serial_port_->Flush(
      mode, mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
}

void SerialConnection::GetControlSignals(
    GetControlSignalsCompleteCallback callback) const {
  DCHECK(serial_port_);
  auto resp_callback = base::BindOnce(
      [](GetControlSignalsCompleteCallback callback,
         device::mojom::SerialPortControlSignalsPtr signals) {
        if (!signals) {
          std::move(callback).Run(nullptr);
          return;
        }
        auto control_signals =
            std::make_unique<api::serial::DeviceControlSignals>();
        control_signals->dcd = signals->dcd;
        control_signals->cts = signals->cts;
        control_signals->ri = signals->ri;
        control_signals->dsr = signals->dsr;
        std::move(callback).Run(std::move(control_signals));
      },
      std::move(callback));
  serial_port_->GetControlSignals(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(resp_callback), nullptr));
}

void SerialConnection::SetControlSignals(
    device::mojom::SerialHostControlSignalsPtr signals,
    SetControlSignalsCompleteCallback callback) {
  DCHECK(serial_port_);
  serial_port_->SetControlSignals(
      std::move(signals),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void SerialConnection::Close(base::OnceClosure callback) {
  DCHECK(serial_port_);
  serial_port_->Close(
      /*flush=*/false,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
}

void SerialConnection::InitSerialPortForTesting() {
  std::ignore = serial_port_.BindNewPipeAndPassReceiver();
}

void SerialConnection::SetTimeoutCallback() {
  if (receive_timeout_ > 0) {
    receive_timeout_task_.Reset(base::BindOnce(
        &SerialConnection::OnReceiveTimeout, weak_factory_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, receive_timeout_task_.callback(),
        base::Milliseconds(receive_timeout_));
  }
}

void SerialConnection::OnReceiveTimeout() {
  DCHECK(serial_port_);
  receive_event_cb_.Run(std::vector<uint8_t>(),
                        api::serial::ReceiveError::kTimeout);
}

void SerialConnection::OnSendTimeout() {
  DCHECK(serial_port_);
  if (send_complete_) {
    send_pipe_watcher_.Cancel();

    // Invoking `send_complete_` may free `this`.
    size_t bytes_written = bytes_written_;
    bytes_written_ = 0;

    // Respond the send request with bytes_written without closing the data
    // pipe.
    std::move(send_complete_)
        .Run(bytes_written, api::serial::SendError::kTimeout);
  }
}

void SerialConnection::OnSendPipeWritableOrClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  // Data pipe disconnected.
  if (result != MOJO_RESULT_OK) {
    OnSendPipeClosed();
    return;
  }
  // There is no send task.
  if (!send_complete_) {
    return;
  }

  DCHECK(send_pipe_);
  size_t actually_sent_bytes = 0;
  result = send_pipe_->WriteData(data_to_send_, MOJO_WRITE_DATA_FLAG_NONE,
                                 actually_sent_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_pipe_watcher_.ArmOrNotify();
    return;
  }

  // For remote pipe producer handle has been closed.
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnSendPipeClosed();
    return;
  }

  // For result == MOJO_RESULT_OK case.
  data_to_send_.erase(data_to_send_.begin(),
                      data_to_send_.begin() + actually_sent_bytes);
  bytes_written_ += actually_sent_bytes;

  if (data_to_send_.empty()) {
    send_timeout_task_.Cancel();
    std::move(send_complete_)
        .Run(bytes_written_, api::serial::SendError::kNone);
  } else {
    // Wait for next cycle to send the remaining bytes.
    send_pipe_watcher_.ArmOrNotify();
  }
}

void SerialConnection::OnSendPipeClosed() {
  OnSendError(device::mojom::SerialSendError::DISCONNECTED);
}

void SerialConnection::OnConnectionError() {
  // Run client's error handler if existing.
  if (connection_error_handler_) {
    std::move(connection_error_handler_).Run();
  }
}

void SerialConnection::OnClientReceiverClosed() {
  client_receiver_.reset();
  OnReadError(device::mojom::SerialReceiveError::DISCONNECTED);
  OnSendError(device::mojom::SerialSendError::DISCONNECTED);
}

}  // namespace extensions

namespace mojo {

// static
device::mojom::SerialHostControlSignalsPtr
TypeConverter<device::mojom::SerialHostControlSignalsPtr,
              extensions::api::serial::HostControlSignals>::
    Convert(const extensions::api::serial::HostControlSignals& input) {
  device::mojom::SerialHostControlSignalsPtr output(
      device::mojom::SerialHostControlSignals::New());
  if (input.dtr) {
    output->has_dtr = true;
    output->dtr = *input.dtr;
  }
  if (input.rts) {
    output->has_rts = true;
    output->rts = *input.rts;
  }
  return output;
}

// static
device::mojom::SerialConnectionOptionsPtr
TypeConverter<device::mojom::SerialConnectionOptionsPtr,
              extensions::api::serial::ConnectionOptions>::
    Convert(const extensions::api::serial::ConnectionOptions& input) {
  device::mojom::SerialConnectionOptionsPtr output(
      device::mojom::SerialConnectionOptions::New());
  if (input.bitrate && *input.bitrate > 0)
    output->bitrate = *input.bitrate;
  output->data_bits = extensions::ConvertDataBitsToMojo(input.data_bits);
  output->parity_bit = extensions::ConvertParityBitToMojo(input.parity_bit);
  output->stop_bits = extensions::ConvertStopBitsToMojo(input.stop_bits);
  if (input.cts_flow_control) {
    output->has_cts_flow_control = true;
    output->cts_flow_control = *input.cts_flow_control;
  }
  return output;
}

}  // namespace mojo
