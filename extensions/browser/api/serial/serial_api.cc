// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_api.h"

#include <algorithm>
#include <map>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/common/api/serial.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace extensions {

namespace api {

namespace {

// It's a fool's errand to come up with a default bitrate, because we don't get
// to control both sides of the communication. Unless the other side has
// implemented auto-bitrate detection (rare), if we pick the wrong rate, then
// you're gonna have a bad time. Close doesn't count.
//
// But we'd like to pick something that has a chance of working, and 9600 is a
// good balance between popularity and speed. So 9600 it is.
const int kDefaultBufferSize = 4096;
const int kDefaultBitrate = 9600;
const serial::DataBits kDefaultDataBits = serial::DATA_BITS_EIGHT;
const serial::ParityBit kDefaultParityBit = serial::PARITY_BIT_NO;
const serial::StopBits kDefaultStopBits = serial::STOP_BITS_ONE;
const int kDefaultReceiveTimeout = 0;
const int kDefaultSendTimeout = 0;

const char kErrorConnectFailed[] = "Failed to connect to the port.";
const char kErrorSerialConnectionNotFound[] = "Serial connection not found.";
const char kErrorGetControlSignalsFailed[] = "Failed to get control signals.";

template <class T>
void SetDefaultScopedPtrValue(std::unique_ptr<T>& ptr, const T& value) {
  if (!ptr.get())
    ptr.reset(new T(value));
}

}  // namespace

SerialExtensionFunction::SerialExtensionFunction() = default;
SerialExtensionFunction::~SerialExtensionFunction() = default;

SerialConnection* SerialExtensionFunction::GetSerialConnection(
    int api_resource_id) {
  auto* manager = ApiResourceManager<SerialConnection>::Get(browser_context());
  return manager->Get(extension_->id(), api_resource_id);
}

void SerialExtensionFunction::RemoveSerialConnection(int api_resource_id) {
  auto* manager = ApiResourceManager<SerialConnection>::Get(browser_context());
  manager->Remove(extension_->id(), api_resource_id);
}

SerialGetDevicesFunction::SerialGetDevicesFunction() = default;
SerialGetDevicesFunction::~SerialGetDevicesFunction() = default;

ExtensionFunction::ResponseAction SerialGetDevicesFunction::Run() {
  auto* port_manager = SerialPortManager::Get(browser_context());
  DCHECK(port_manager);
  port_manager->GetDevices(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&SerialGetDevicesFunction::OnGotDevices, this),
      std::vector<device::mojom::SerialPortInfoPtr>()));
  return RespondLater();
}

void SerialGetDevicesFunction::OnGotDevices(
    std::vector<device::mojom::SerialPortInfoPtr> devices) {
  Respond(ArgumentList(serial::GetDevices::Results::Create(
      mojo::ConvertTo<std::vector<serial::DeviceInfo>>(devices))));
}

SerialConnectFunction::SerialConnectFunction() {}

SerialConnectFunction::~SerialConnectFunction() {}

ExtensionFunction::ResponseAction SerialConnectFunction::Run() {
  auto params = serial::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Fill in any omitted options to ensure a known initial configuration.
  if (!params->options)
    params->options = std::make_unique<serial::ConnectionOptions>();
  serial::ConnectionOptions* options = params->options.get();

  SetDefaultScopedPtrValue(options->persistent, false);
  SetDefaultScopedPtrValue(options->buffer_size, kDefaultBufferSize);
  SetDefaultScopedPtrValue(options->bitrate, kDefaultBitrate);
  SetDefaultScopedPtrValue(options->cts_flow_control, false);
  SetDefaultScopedPtrValue(options->receive_timeout, kDefaultReceiveTimeout);
  SetDefaultScopedPtrValue(options->send_timeout, kDefaultSendTimeout);

  if (options->data_bits == serial::DATA_BITS_NONE)
    options->data_bits = kDefaultDataBits;
  if (options->parity_bit == serial::PARITY_BIT_NONE)
    options->parity_bit = kDefaultParityBit;
  if (options->stop_bits == serial::STOP_BITS_NONE)
    options->stop_bits = kDefaultStopBits;

  auto* manager = SerialPortManager::Get(browser_context());
  DCHECK(manager);

  mojo::PendingRemote<device::mojom::SerialPort> serial_port;
  manager->GetPort(params->path, serial_port.InitWithNewPipeAndPassReceiver());

  connection_ = std::make_unique<SerialConnection>(extension_->id(),
                                                   std::move(serial_port));
  connection_->Open(*params->options,
                    base::BindOnce(&SerialConnectFunction::OnConnected, this));
  return RespondLater();
}

void SerialConnectFunction::OnConnected(bool success) {
  DCHECK(connection_);

  if (!success) {
    FinishConnect(false, false, nullptr);
    return;
  }

  connection_->GetInfo(
      base::BindOnce(&SerialConnectFunction::FinishConnect, this, true));
}

void SerialConnectFunction::FinishConnect(
    bool connected,
    bool got_complete_info,
    std::unique_ptr<serial::ConnectionInfo> info) {
  DCHECK(connection_);
  if (!connected || !got_complete_info) {
    Respond(Error(kErrorConnectFailed));
    connection_.reset();
  } else {
    DCHECK(info);
    auto* manager =
        ApiResourceManager<SerialConnection>::Get(browser_context());
    int id = manager->Add(connection_.release());
    // If a SerialConnection encountered a mojo connection error, it just
    // becomes useless, we won't try to re-connect it but just remove it
    // completely.
    SerialConnection* connection = GetSerialConnection(id);
    connection->SetConnectionErrorHandler(base::BindOnce(
        [](scoped_refptr<ApiResourceManager<SerialConnection>::ApiResourceData>
               connections,
           std::string extension_id, int api_resource_id) {
          connections->Remove(extension_id, api_resource_id);
        },
        manager->data_, extension_->id(), id));
    info->connection_id = id;
    // Start polling.
    auto* port_manager = SerialPortManager::Get(browser_context());
    port_manager->StartConnectionPolling(extension_->id(), id);
    Respond(OneArgument(info->ToValue()));
  }
}

SerialUpdateFunction::SerialUpdateFunction() = default;
SerialUpdateFunction::~SerialUpdateFunction() = default;

ExtensionFunction::ResponseAction SerialUpdateFunction::Run() {
  auto params = serial::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->Configure(params->options,
                        base::BindOnce(&SerialUpdateFunction::OnUpdated, this));
  return RespondLater();
}

void SerialUpdateFunction::OnUpdated(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

SerialDisconnectFunction::SerialDisconnectFunction() = default;
SerialDisconnectFunction::~SerialDisconnectFunction() = default;

ExtensionFunction::ResponseAction SerialDisconnectFunction::Run() {
  auto params = serial::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->Close(base::BindOnce(&SerialDisconnectFunction::OnCloseComplete,
                                   this, params->connection_id));
  return RespondLater();
}

void SerialDisconnectFunction::OnCloseComplete(int connection_id) {
  RemoveSerialConnection(connection_id);
  Respond(OneArgument(std::make_unique<base::Value>(true)));
}

SerialSendFunction::SerialSendFunction() = default;
SerialSendFunction::~SerialSendFunction() = default;

ExtensionFunction::ResponseAction SerialSendFunction::Run() {
  auto params = serial::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  if (!connection->Send(
          params->data,
          base::BindOnce(&SerialSendFunction::OnSendComplete, this))) {
    serial::SendInfo send_info;
    send_info.bytes_sent = 0;
    send_info.error = serial::SEND_ERROR_PENDING;
    return RespondNow(OneArgument(send_info.ToValue()));
  }
  return RespondLater();
}

void SerialSendFunction::OnSendComplete(uint32_t bytes_sent,
                                        serial::SendError error) {
  serial::SendInfo send_info;
  send_info.bytes_sent = bytes_sent;
  send_info.error = error;
  Respond(OneArgument(send_info.ToValue()));
}

SerialFlushFunction::SerialFlushFunction() = default;
SerialFlushFunction::~SerialFlushFunction() = default;

ExtensionFunction::ResponseAction SerialFlushFunction::Run() {
  auto params = serial::Flush::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->Flush(base::BindOnce(&SerialFlushFunction::OnFlushed, this));
  return RespondLater();
}

void SerialFlushFunction::OnFlushed(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

SerialSetPausedFunction::SerialSetPausedFunction() = default;
SerialSetPausedFunction::~SerialSetPausedFunction() = default;

ExtensionFunction::ResponseAction SerialSetPausedFunction::Run() {
  auto params = serial::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  if (params->paused != connection->paused())
    connection->SetPaused(params->paused);

  return RespondNow(NoArguments());
}

SerialGetInfoFunction::SerialGetInfoFunction() = default;
SerialGetInfoFunction::~SerialGetInfoFunction() = default;

ExtensionFunction::ResponseAction SerialGetInfoFunction::Run() {
  auto params = serial::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->GetInfo(base::BindOnce(&SerialGetInfoFunction::OnGotInfo, this,
                                     params->connection_id));
  return RespondLater();
}

void SerialGetInfoFunction::OnGotInfo(
    int connection_id,
    bool got_complete_info,
    std::unique_ptr<serial::ConnectionInfo> info) {
  DCHECK(info);
  info->connection_id = connection_id;
  Respond(OneArgument(info->ToValue()));
}

SerialGetConnectionsFunction::SerialGetConnectionsFunction() = default;
SerialGetConnectionsFunction::~SerialGetConnectionsFunction() = default;

ExtensionFunction::ResponseAction SerialGetConnectionsFunction::Run() {
  auto* manager = ApiResourceManager<SerialConnection>::Get(browser_context());
  const std::unordered_set<int>* connection_ids =
      manager->GetResourceIds(extension_->id());
  if (connection_ids) {
    for (auto it = connection_ids->cbegin(); it != connection_ids->cend();
         ++it) {
      int connection_id = *it;
      SerialConnection* connection = GetSerialConnection(connection_id);
      if (connection) {
        count_++;
        connection->GetInfo(base::BindOnce(
            &SerialGetConnectionsFunction::OnGotOne, this, connection_id));
      }
    }
  }

  if (count_ > 0)
    return RespondLater();

  return RespondNow(ArgumentList(serial::GetConnections::Results::Create(
      std::vector<serial::ConnectionInfo>())));
}

void SerialGetConnectionsFunction::OnGotOne(
    int connection_id,
    bool got_complete_info,
    std::unique_ptr<serial::ConnectionInfo> info) {
  DCHECK(info);
  info->connection_id = connection_id;
  infos_.push_back(std::move(*info));

  if (infos_.size() == count_)
    Respond(ArgumentList(serial::GetConnections::Results::Create(infos_)));
}

SerialGetControlSignalsFunction::SerialGetControlSignalsFunction() = default;
SerialGetControlSignalsFunction::~SerialGetControlSignalsFunction() = default;

ExtensionFunction::ResponseAction SerialGetControlSignalsFunction::Run() {
  auto params = serial::GetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->GetControlSignals(base::BindOnce(
      &SerialGetControlSignalsFunction::OnGotControlSignals, this));
  return RespondLater();
}

void SerialGetControlSignalsFunction::OnGotControlSignals(
    std::unique_ptr<serial::DeviceControlSignals> signals) {
  if (!signals) {
    Respond(Error(kErrorGetControlSignalsFailed));
  } else {
    Respond(OneArgument(signals->ToValue()));
  }
}

SerialSetControlSignalsFunction::SerialSetControlSignalsFunction() = default;
SerialSetControlSignalsFunction::~SerialSetControlSignalsFunction() = default;

ExtensionFunction::ResponseAction SerialSetControlSignalsFunction::Run() {
  auto params = serial::SetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  connection->SetControlSignals(
      device::mojom::SerialHostControlSignals::From(params->signals),
      base::BindOnce(&SerialSetControlSignalsFunction::OnSetControlSignals,
                     this));
  return RespondLater();
}

void SerialSetControlSignalsFunction::OnSetControlSignals(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

SerialSetBreakFunction::SerialSetBreakFunction() = default;
SerialSetBreakFunction::~SerialSetBreakFunction() = default;

ExtensionFunction::ResponseAction SerialSetBreakFunction::Run() {
  auto params = serial::SetBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  auto signals = device::mojom::SerialHostControlSignals::New();
  signals->has_brk = true;
  signals->brk = true;
  connection->SetControlSignals(
      std::move(signals),
      base::BindOnce(&SerialSetBreakFunction::OnSetBreak, this));
  return RespondLater();
}

void SerialSetBreakFunction::OnSetBreak(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

SerialClearBreakFunction::SerialClearBreakFunction() = default;
SerialClearBreakFunction::~SerialClearBreakFunction() = default;

ExtensionFunction::ResponseAction SerialClearBreakFunction::Run() {
  auto params = serial::ClearBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  SerialConnection* connection = GetSerialConnection(params->connection_id);
  if (!connection)
    return RespondNow(Error(kErrorSerialConnectionNotFound));

  auto signals = device::mojom::SerialHostControlSignals::New();
  signals->has_brk = true;
  signals->brk = false;
  connection->SetControlSignals(
      std::move(signals),
      base::BindOnce(&SerialClearBreakFunction::OnClearBreak, this));
  return RespondLater();
}

void SerialClearBreakFunction::OnClearBreak(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

}  // namespace api

}  // namespace extensions

namespace mojo {

// static
extensions::api::serial::DeviceInfo
TypeConverter<extensions::api::serial::DeviceInfo,
              device::mojom::SerialPortInfoPtr>::
    Convert(const device::mojom::SerialPortInfoPtr& device) {
  extensions::api::serial::DeviceInfo info;
  info.path = device->path.AsUTF8Unsafe();
  if (device->has_vendor_id)
    info.vendor_id.reset(new int(static_cast<int>(device->vendor_id)));
  if (device->has_product_id)
    info.product_id.reset(new int(static_cast<int>(device->product_id)));
  if (device->display_name)
    info.display_name.reset(new std::string(device->display_name.value()));
  return info;
}

}  // namespace mojo
