// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_dbus_client.h"

#include <string>

#include "base/logging.h"
#include "dbus/message.h"

namespace floss {

// TODO(b/189499077) - Expose via floss package
const char kAdapterService[] = "org.chromium.bluetooth";
const char kAdapterInterface[] = "org.chromium.bluetooth.Bluetooth";
const char kManagerInterface[] = "org.chromium.bluetooth.Manager";
const char kManagerObject[] = "/org/chromium/bluetooth/Manager";
const char kAdapterObjectFormat[] = "/org/chromium/bluetooth/hci%d/adapter";

namespace adapter {
const char kGetAddress[] = "GetAddress";
const char kStartDiscovery[] = "StartDiscovery";
const char kCancelDiscovery[] = "CancelDiscovery";
const char kCreateBond[] = "CreateBond";
const char kRegisterCallback[] = "RegisterCallback";

// TODO(abps) - Rename this to AdapterCallback in platform and here
const char kCallbackInterface[] = "org.chromium.bluetooth.BluetoothCallback";

const char kOnAddressChanged[] = "OnBluetoothAddressChanged";
const char kOnDeviceFound[] = "OnDeviceFound";
const char kOnDiscoveringChanged[] = "OnDiscoveringChanged";
const char kOnSspRequest[] = "OnSspRequest";
}  // namespace adapter

namespace manager {
const char kStart[] = "Start";
const char kStop[] = "Stop";
const char kGetFlossEnabled[] = "GetFlossEnabled";
const char kSetFlossEnabled[] = "SetFlossEnabled";
const char kGetState[] = "GetState";
const char kGetAvailableAdapters[] = "GetAvailableAdapters";
const char kRegisterCallback[] = "RegisterCallback";
const char kCallbackInterface[] = "org.chromium.bluetooth.ManagerCallback";
const char kOnHciDeviceChanged[] = "OnHciDeviceChanged";
const char kOnHciEnabledChanged[] = "OnHciEnabledChanged";
}  // namespace manager

Error::Error(const std::string& name, const std::string& message)
    : name(name), message(message) {}

FlossDBusClient::FlossDBusClient() = default;
FlossDBusClient::~FlossDBusClient() = default;

const char FlossDBusClient::kErrorNoResponse[] =
    "org.chromium.Error.NoResponse";
const char FlossDBusClient::kErrorInvalidParameters[] =
    "org.chromium.Error.InvalidParameters";

// Default error handler for dbus clients is to just print the error right now.
// TODO(abps) - Deprecate this once error handling is implemented in the upper
//              layers.
void FlossDBusClient::LogErrorResponse(const std::string& message,
                                       dbus::ErrorResponse* error) {
  if (!error) {
    return;
  }

  dbus::MessageReader reader(error);
  auto error_name = error->GetErrorName();
  std::string error_message;
  reader.PopString(&error_message);

  LOG(ERROR) << message << ": " << error_name << ": " << error_message;
}

// static
Error FlossDBusClient::ErrorResponseToError(const std::string& default_name,
                                            const std::string& default_message,
                                            dbus::ErrorResponse* error) {
  Error result(default_name, default_message);

  if (error) {
    dbus::MessageReader reader(error);
    result.name = error->GetErrorName();
    reader.PopString(&result.message);
  }

  return result;
}

void FlossDBusClient::DefaultResponseWithCallback(
    ResponseCallback callback,
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  if (response) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(ErrorResponseToError(
      kErrorNoResponse, /*default_message=*/std::string(), error_response));
}

void FlossDBusClient::DefaultResponse(const std::string& caller,
                                      dbus::Response* response,
                                      dbus::ErrorResponse* error_response) {
  if (error_response) {
    FlossDBusClient::LogErrorResponse(caller, error_response);
  } else {
    DVLOG(1) << caller << "::OnResponse";
  }
}

}  // namespace floss
