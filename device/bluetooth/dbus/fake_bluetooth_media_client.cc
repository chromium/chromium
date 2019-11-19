// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_media_client.h"

#include <string>

#include "base/stl_util.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_endpoint_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_transport_client.h"

using dbus::ObjectPath;

namespace {

// Except for |kFailedError|, the other error is defined in BlueZ D-Bus Media
// API.
const char kFailedError[] = "org.chromium.Error.Failed";
const char kInvalidArgumentsError[] = "org.chromium.Error.InvalidArguments";

}  // namespace

namespace bluez {

// static
const uint8_t FakeBluetoothMediaClient::kDefaultCodec = 0x00;

FakeBluetoothMediaClient::FakeBluetoothMediaClient()
    : visible_(true),
      object_path_(ObjectPath(FakeBluetoothAdapterClient::kAdapterPath)) {}

FakeBluetoothMediaClient::~FakeBluetoothMediaClient() = default;

void FakeBluetoothMediaClient::Init(dbus::Bus* bus,
                                    const std::string& bluetooth_service_name) {
}

void FakeBluetoothMediaClient::AddObserver(
    BluetoothMediaClient::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeBluetoothMediaClient::RemoveObserver(
    BluetoothMediaClient::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void FakeBluetoothMediaClient::RegisterEndpoint(
    const ObjectPath& object_path,
    const ObjectPath& endpoint_path,
    const EndpointProperties& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!visible_)
    return;

  VLOG(1) << "RegisterEndpoint: " << endpoint_path.value();

  // The media client and adapter client should have the same object path.
  if (object_path != object_path_ ||
      properties.uuid != BluetoothMediaClient::kBluetoothAudioSinkUUID ||
      properties.codec != kDefaultCodec || properties.capabilities.empty()) {
    error_callback.Run(kInvalidArgumentsError, "");
    return;
  }

  callback.Run();
}

void FakeBluetoothMediaClient::UnregisterEndpoint(
    const ObjectPath& object_path,
    const ObjectPath& endpoint_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(mcchou): Come up with some corresponding actions.
  VLOG(1) << "UnregisterEndpoint: " << endpoint_path.value();

  if (!base::Contains(endpoints_, endpoint_path)) {
    error_callback.Run(kFailedError, "Unknown media endpoint");
    return;
  }

  SetEndpointRegistered(endpoints_[endpoint_path], false);
  callback.Run();
}

void FakeBluetoothMediaClient::SetVisible(bool visible) {
  visible_ = visible;

  if (visible_)
    return;

  // If the media object becomes invisible, an update chain will unregister all
  // endpoints and set the associated transport objects to be invalid.
  // SetEndpointRegistered will remove the endpoint entry from |endpoints_|.
  while (endpoints_.begin() != endpoints_.end())
    SetEndpointRegistered(endpoints_.begin()->second, false);

  // Notifies observers about the change on |visible_|.
  for (auto& observer : observers_)
    observer.MediaRemoved(object_path_);
}

void FakeBluetoothMediaClient::SetEndpointRegistered(
    FakeBluetoothMediaEndpointServiceProvider* endpoint,
    bool registered) {
  if (registered) {
    endpoints_[endpoint->object_path()] = endpoint;
    return;
  }

  if (!IsRegistered(endpoint->object_path()))
    return;

  // Once a media endpoint object becomes invalid, invalidate the associated
  // transport.
  FakeBluetoothMediaTransportClient* transport =
      static_cast<FakeBluetoothMediaTransportClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothMediaTransportClient());
  transport->SetValid(endpoint, false);

  endpoints_.erase(endpoint->object_path());
  endpoint->Released();
}

bool FakeBluetoothMediaClient::IsRegistered(
    const dbus::ObjectPath& endpoint_path) {
  return base::Contains(endpoints_, endpoint_path);
}

}  // namespace bluez
