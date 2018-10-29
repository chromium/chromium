// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/serial/serial_api.h"

#include <algorithm>
#include <vector>

#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_event_dispatcher.h"
#include "extensions/common/api/serial.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using content::BrowserThread;

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

SerialAsyncApiFunction::SerialAsyncApiFunction() : manager_(nullptr) {}

SerialAsyncApiFunction::~SerialAsyncApiFunction() {}

bool SerialAsyncApiFunction::PrePrepare() {
  manager_ = ApiResourceManager<SerialConnection>::Get(browser_context());
  DCHECK(manager_);
  return true;
}

bool SerialAsyncApiFunction::Respond() {
  return error_.empty();
}

SerialConnection* SerialAsyncApiFunction::GetSerialConnection(
    int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SerialAsyncApiFunction::RemoveSerialConnection(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

SerialGetDevicesFunction::SerialGetDevicesFunction() {}

SerialGetDevicesFunction::~SerialGetDevicesFunction() {}

ExtensionFunction::ResponseAction SerialGetDevicesFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content::ServiceManagerConnection::GetForProcess());
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&enumerator_));
  enumerator_.set_connection_error_handler(
      base::BindOnce(&SerialGetDevicesFunction::OnGotDevices, this,
                     std::vector<device::mojom::SerialDeviceInfoPtr>()));
  enumerator_->GetDevices(
      base::BindOnce(&SerialGetDevicesFunction::OnGotDevices, this));
  return RespondLater();
}

void SerialGetDevicesFunction::OnGotDevices(
    std::vector<device::mojom::SerialDeviceInfoPtr> devices) {
  std::unique_ptr<base::ListValue> results =
      serial::GetDevices::Results::Create(
          mojo::ConvertTo<std::vector<serial::DeviceInfo>>(devices));
  Respond(ArgumentList(std::move(results)));
}

SerialConnectFunction::SerialConnectFunction() {}

SerialConnectFunction::~SerialConnectFunction() {}

bool SerialConnectFunction::Prepare() {
  params_ = serial::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  // Fill in any omitted options to ensure a known initial configuration.
  if (!params_->options.get())
    params_->options.reset(new serial::ConnectionOptions());
  serial::ConnectionOptions* options = params_->options.get();

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

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(content::ServiceManagerConnection::GetForProcess());
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(device::mojom::kServiceName,
                      mojo::MakeRequest(&io_handler_info_));

  serial_event_dispatcher_ = SerialEventDispatcher::Get(browser_context());
  DCHECK(serial_event_dispatcher_);

  return true;
}

void SerialConnectFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  connection_ = std::make_unique<SerialConnection>(
      params_->path, extension_->id(), std::move(io_handler_info_));
  connection_->Open(*params_->options,
                    base::BindOnce(&SerialConnectFunction::OnConnected, this));
}

void SerialConnectFunction::OnConnected(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(connection_);
  if (!connected || !got_complete_info) {
    error_ = kErrorConnectFailed;
    connection_.reset();
  } else {
    DCHECK(info);
    int id = manager_->Add(connection_.release());
    // If a SerialConnection encountered a mojo connection error, it just
    // becomes useless, we won't try to re-connect it but just remove it
    // completely.
    GetSerialConnection(id)->set_connection_error_handler(base::BindOnce(
        [](scoped_refptr<ApiResourceManager<SerialConnection>::ApiResourceData>
               connections,
           std::string extension_id, int api_resource_id) {
          connections->Remove(extension_id, api_resource_id);
        },
        manager_->data_, extension_->id(), id));

    info->connection_id = id;
    serial_event_dispatcher_->PollConnection(extension_->id(), id);
    results_ = serial::Connect::Results::Create(*info);
  }
  AsyncWorkCompleted();
}

SerialUpdateFunction::SerialUpdateFunction() {}

SerialUpdateFunction::~SerialUpdateFunction() {}

bool SerialUpdateFunction::Prepare() {
  params_ = serial::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialUpdateFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }
  connection->Configure(params_->options,
                        base::BindOnce(&SerialUpdateFunction::OnUpdated, this));
}

void SerialUpdateFunction::OnUpdated(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  results_ = serial::Update::Results::Create(success);
  AsyncWorkCompleted();
}

SerialDisconnectFunction::SerialDisconnectFunction() {}

SerialDisconnectFunction::~SerialDisconnectFunction() {}

bool SerialDisconnectFunction::Prepare() {
  params_ = serial::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialDisconnectFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }
  RemoveSerialConnection(params_->connection_id);
  results_ = serial::Disconnect::Results::Create(true);
}

SerialSendFunction::SerialSendFunction() {}

SerialSendFunction::~SerialSendFunction() {}

bool SerialSendFunction::Prepare() {
  params_ = serial::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSendFunction::AsyncWorkStart() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }

  if (!connection->Send(
          params_->data,
          base::BindOnce(&SerialSendFunction::OnSendComplete, this))) {
    OnSendComplete(0, serial::SEND_ERROR_PENDING);
  }
}

void SerialSendFunction::OnSendComplete(uint32_t bytes_sent,
                                        serial::SendError error) {
  serial::SendInfo send_info;
  send_info.bytes_sent = bytes_sent;
  send_info.error = error;
  results_ = serial::Send::Results::Create(send_info);
  AsyncWorkCompleted();
}

SerialFlushFunction::SerialFlushFunction() {}

SerialFlushFunction::~SerialFlushFunction() {}

bool SerialFlushFunction::Prepare() {
  params_ = serial::Flush::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SerialFlushFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }
  connection->Flush(base::BindOnce(&SerialFlushFunction::OnFlushed, this));
}

void SerialFlushFunction::OnFlushed(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  results_ = serial::Flush::Results::Create(success);
  AsyncWorkCompleted();
}

SerialSetPausedFunction::SerialSetPausedFunction() {}

SerialSetPausedFunction::~SerialSetPausedFunction() {}

bool SerialSetPausedFunction::Prepare() {
  params_ = serial::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  serial_event_dispatcher_ = SerialEventDispatcher::Get(browser_context());
  DCHECK(serial_event_dispatcher_);
  return true;
}

void SerialSetPausedFunction::Work() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    return;
  }

  if (params_->paused != connection->paused()) {
    connection->set_paused(params_->paused);
    if (!params_->paused) {
      serial_event_dispatcher_->PollConnection(extension_->id(),
                                               params_->connection_id);
    }
  }

  results_ = serial::SetPaused::Results::Create();
}

SerialGetInfoFunction::SerialGetInfoFunction() {}

SerialGetInfoFunction::~SerialGetInfoFunction() {}

bool SerialGetInfoFunction::Prepare() {
  params_ = serial::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialGetInfoFunction::AsyncWorkStart() {
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }

  connection->GetInfo(base::BindOnce(&SerialGetInfoFunction::OnGotInfo, this));
}

void SerialGetInfoFunction::OnGotInfo(
    bool got_complete_info,
    std::unique_ptr<serial::ConnectionInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(info);
  info->connection_id = params_->connection_id;
  results_ = serial::GetInfo::Results::Create(*info);

  AsyncWorkCompleted();
}

SerialGetConnectionsFunction::SerialGetConnectionsFunction() {}

SerialGetConnectionsFunction::~SerialGetConnectionsFunction() {}

bool SerialGetConnectionsFunction::Prepare() {
  return true;
}

void SerialGetConnectionsFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const base::hash_set<int>* connection_ids =
      manager_->GetResourceIds(extension_->id());
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
    return;

  OnGotAll();
}

void SerialGetConnectionsFunction::OnGotOne(
    int connection_id,
    bool got_complete_info,
    std::unique_ptr<serial::ConnectionInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(info);
  info->connection_id = connection_id;
  infos_.push_back(std::move(*info));

  if (infos_.size() == count_) {
    OnGotAll();
  }
}

void SerialGetConnectionsFunction::OnGotAll() {
  results_ = serial::GetConnections::Results::Create(infos_);
  AsyncWorkCompleted();
}

SerialGetControlSignalsFunction::SerialGetControlSignalsFunction() {}

SerialGetControlSignalsFunction::~SerialGetControlSignalsFunction() {}

bool SerialGetControlSignalsFunction::Prepare() {
  params_ = serial::GetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialGetControlSignalsFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }

  connection->GetControlSignals(base::BindOnce(
      &SerialGetControlSignalsFunction::OnGotControlSignals, this));
}

void SerialGetControlSignalsFunction::OnGotControlSignals(
    std::unique_ptr<serial::DeviceControlSignals> signals) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!signals) {
    error_ = kErrorGetControlSignalsFailed;
  } else {
    results_ = serial::GetControlSignals::Results::Create(*signals);
  }

  AsyncWorkCompleted();
}

SerialSetControlSignalsFunction::SerialSetControlSignalsFunction() {}

SerialSetControlSignalsFunction::~SerialSetControlSignalsFunction() {}

bool SerialSetControlSignalsFunction::Prepare() {
  params_ = serial::SetControlSignals::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSetControlSignalsFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }

  connection->SetControlSignals(
      params_->signals,
      base::BindOnce(&SerialSetControlSignalsFunction::OnSetControlSignals,
                     this));
}

void SerialSetControlSignalsFunction::OnSetControlSignals(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  results_ = serial::SetControlSignals::Results::Create(success);
  AsyncWorkCompleted();
}

SerialSetBreakFunction::SerialSetBreakFunction() {}

SerialSetBreakFunction::~SerialSetBreakFunction() {}

bool SerialSetBreakFunction::Prepare() {
  params_ = serial::SetBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialSetBreakFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }
  connection->SetBreak(
      base::BindOnce(&SerialSetBreakFunction::OnSetBreak, this));
}

void SerialSetBreakFunction::OnSetBreak(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  results_ = serial::SetBreak::Results::Create(success);
  AsyncWorkCompleted();
}

SerialClearBreakFunction::SerialClearBreakFunction() {}

SerialClearBreakFunction::~SerialClearBreakFunction() {}

bool SerialClearBreakFunction::Prepare() {
  params_ = serial::ClearBreak::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  return true;
}

void SerialClearBreakFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SerialConnection* connection = GetSerialConnection(params_->connection_id);
  if (!connection) {
    error_ = kErrorSerialConnectionNotFound;
    AsyncWorkCompleted();
    return;
  }
  connection->ClearBreak(
      base::BindOnce(&SerialClearBreakFunction::OnClearBreak, this));
}

void SerialClearBreakFunction::OnClearBreak(bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  results_ = serial::ClearBreak::Results::Create(success);
  AsyncWorkCompleted();
}

}  // namespace api

}  // namespace extensions

namespace mojo {

// static
extensions::api::serial::DeviceInfo
TypeConverter<extensions::api::serial::DeviceInfo,
              device::mojom::SerialDeviceInfoPtr>::
    Convert(const device::mojom::SerialDeviceInfoPtr& device) {
  extensions::api::serial::DeviceInfo info;
  info.path = device->path;
  if (device->has_vendor_id)
    info.vendor_id.reset(new int(static_cast<int>(device->vendor_id)));
  if (device->has_product_id)
    info.product_id.reset(new int(static_cast<int>(device->product_id)));
  if (device->display_name)
    info.display_name.reset(new std::string(device->display_name.value()));
  return info;
}

}  // namespace mojo
