// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_media_transport_client.h"

#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/file_descriptor_posix.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "device/bluetooth/dbus/bluetooth_media_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_endpoint_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using dbus::ObjectPath;

namespace {

const char kNotImplemented[] = "org.bluez.NotImplemented";
const char kNotAuthorized[] = "org.bluez.NotAuthorized";
const char kFailed[] = "org.bluez.Failed";
const char kNotAvailable[] = "org.bluez.NotAvailable";

ObjectPath GenerateTransportPath() {
  static unsigned int sequence_number = 0;
  ++sequence_number;
  std::stringstream path;
  path << bluez::FakeBluetoothAdapterClient::kAdapterPath
       << bluez::FakeBluetoothMediaTransportClient::kTransportDevicePath
       << "/fd" << sequence_number;
  return ObjectPath(path.str());
}

#define UINT8_VECTOR_FROM_ARRAY(array) \
  std::vector<uint8_t>(array, array + base::size(array))

}  // namespace

namespace bluez {

// static
const char FakeBluetoothMediaTransportClient::kTransportDevicePath[] =
    "/fake_audio_source";
const uint8_t FakeBluetoothMediaTransportClient::kTransportCodec = 0x00;
const uint8_t FakeBluetoothMediaTransportClient::kTransportConfiguration[] = {
    0x21, 0x15, 0x33, 0x2C};
const uint8_t FakeBluetoothMediaTransportClient::kTransportConfigurationLength =
    base::size(FakeBluetoothMediaTransportClient::kTransportConfiguration);
const uint16_t FakeBluetoothMediaTransportClient::kTransportDelay = 5;
const uint16_t FakeBluetoothMediaTransportClient::kTransportVolume = 50;
const uint16_t FakeBluetoothMediaTransportClient::kDefaultReadMtu = 20;
const uint16_t FakeBluetoothMediaTransportClient::kDefaultWriteMtu = 25;

FakeBluetoothMediaTransportClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothMediaTransportClient::Properties(
          nullptr,
          bluetooth_media_transport::kBluetoothMediaTransportInterface,
          callback) {}

FakeBluetoothMediaTransportClient::Properties::~Properties() = default;

void FakeBluetoothMediaTransportClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothMediaTransportClient::Properties::GetAll() {
  VLOG(1) << "GetAll called.";
}

void FakeBluetoothMediaTransportClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothMediaTransportClient::Transport::Transport(
    const ObjectPath& transport_path,
    std::unique_ptr<Properties> transport_properties)
    : path(transport_path), properties(std::move(transport_properties)) {}

FakeBluetoothMediaTransportClient::Transport::~Transport() = default;

FakeBluetoothMediaTransportClient::FakeBluetoothMediaTransportClient() =
    default;

FakeBluetoothMediaTransportClient::~FakeBluetoothMediaTransportClient() =
    default;

// DBusClient override.
void FakeBluetoothMediaTransportClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothMediaTransportClient::AddObserver(
    BluetoothMediaTransportClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothMediaTransportClient::RemoveObserver(
    BluetoothMediaTransportClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeBluetoothMediaTransportClient::Properties*
FakeBluetoothMediaTransportClient::GetProperties(
    const ObjectPath& object_path) {
  const ObjectPath& endpoint_path = GetEndpointPath(object_path);
  Transport* transport = GetTransport(endpoint_path);
  if (!transport)
    return nullptr;
  return transport->properties.get();
}

void FakeBluetoothMediaTransportClient::Acquire(
    const ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Acquire - transport path: " << object_path.value();
  AcquireInternal(false, object_path, callback, error_callback);
}

void FakeBluetoothMediaTransportClient::TryAcquire(
    const ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "TryAcquire - transport path: " << object_path.value();
  AcquireInternal(true, object_path, callback, error_callback);
}

void FakeBluetoothMediaTransportClient::Release(
    const ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::SetValid(
    FakeBluetoothMediaEndpointServiceProvider* endpoint,
    bool valid) {
  FakeBluetoothMediaClient* media = static_cast<FakeBluetoothMediaClient*>(
      bluez::BluezDBusManager::Get()->GetBluetoothMediaClient());
  DCHECK(media);

  ObjectPath endpoint_path(endpoint->object_path());
  if (!media->IsRegistered(endpoint_path))
    return;

  if (valid) {
    ObjectPath transport_path = GenerateTransportPath();
    VLOG(1) << "New transport, " << transport_path.value()
            << " is created for endpoint " << endpoint_path.value();

    // Sets the fake property set with default values.
    std::unique_ptr<Properties> properties(new Properties(
        base::Bind(&FakeBluetoothMediaTransportClient::OnPropertyChanged,
                   base::Unretained(this))));
    properties->device.ReplaceValue(ObjectPath(kTransportDevicePath));
    properties->uuid.ReplaceValue(
        BluetoothMediaClient::kBluetoothAudioSinkUUID);
    properties->codec.ReplaceValue(kTransportCodec);
    properties->configuration.ReplaceValue(
        UINT8_VECTOR_FROM_ARRAY(kTransportConfiguration));
    properties->state.ReplaceValue(BluetoothMediaTransportClient::kStateIdle);
    properties->delay.ReplaceValue(kTransportDelay);
    properties->volume.ReplaceValue(kTransportVolume);

    endpoint_to_transport_map_[endpoint_path] =
        std::make_unique<Transport>(transport_path, std::move(properties));
    transport_to_endpoint_map_[transport_path] = endpoint_path;
    return;
  }

  Transport* transport = GetTransport(endpoint_path);
  if (!transport)
    return;
  ObjectPath transport_path = transport->path;

  // Notifies observers about the state change of the transport.
  for (auto& observer : observers_)
    observer.MediaTransportRemoved(transport_path);

  endpoint->ClearConfiguration(transport_path);
  endpoint_to_transport_map_.erase(endpoint_path);
  transport_to_endpoint_map_.erase(transport_path);
}

void FakeBluetoothMediaTransportClient::SetState(
    const ObjectPath& endpoint_path,
    const std::string& state) {
  VLOG(1) << "SetState - state: " << state;

  Transport* transport = GetTransport(endpoint_path);
  if (!transport)
    return;

  transport->properties->state.ReplaceValue(state);
  for (auto& observer : observers_) {
    observer.MediaTransportPropertyChanged(
        transport->path, BluetoothMediaTransportClient::kStateProperty);
  }
}

void FakeBluetoothMediaTransportClient::SetVolume(
    const ObjectPath& endpoint_path,
    const uint16_t& volume) {
  Transport* transport = GetTransport(endpoint_path);
  if (!transport)
    return;

  transport->properties->volume.ReplaceValue(volume);
  for (auto& observer : observers_) {
    observer.MediaTransportPropertyChanged(
        transport->path, BluetoothMediaTransportClient::kVolumeProperty);
  }
}

void FakeBluetoothMediaTransportClient::WriteData(
    const ObjectPath& endpoint_path,
    const std::vector<char>& bytes) {
  VLOG(1) << "WriteData - write " << bytes.size() << " bytes";

  Transport* transport = GetTransport(endpoint_path);

  if (!transport || transport->properties->state.value() != "active") {
    VLOG(1) << "WriteData - write operation rejected, since the state isn't "
               "active for endpoint: "
            << endpoint_path.value();
    return;
  }

  if (!transport->input_fd.get()) {
    VLOG(1) << "WriteData - invalid input file descriptor";
    return;
  }

  ssize_t written_len =
      write(transport->input_fd->GetPlatformFile(), bytes.data(), bytes.size());
  if (written_len < 0) {
    VLOG(1) << "WriteData - failed to write to the socket";
    return;
  }

  VLOG(1) << "WriteData - wrote " << written_len << " bytes to the socket";
}

ObjectPath FakeBluetoothMediaTransportClient::GetTransportPath(
    const ObjectPath& endpoint_path) {
  Transport* transport = GetTransport(endpoint_path);
  return transport ? transport->path : ObjectPath("");
}

void FakeBluetoothMediaTransportClient::OnPropertyChanged(
    const std::string& property_name) {
  VLOG(1) << "Property " << property_name << " changed";
}

ObjectPath FakeBluetoothMediaTransportClient::GetEndpointPath(
    const ObjectPath& transport_path) {
  const auto& it = transport_to_endpoint_map_.find(transport_path);
  return it != transport_to_endpoint_map_.end() ? it->second : ObjectPath("");
}

FakeBluetoothMediaTransportClient::Transport*
FakeBluetoothMediaTransportClient::GetTransport(
    const ObjectPath& endpoint_path) {
  const auto& it = endpoint_to_transport_map_.find(endpoint_path);
  return (it != endpoint_to_transport_map_.end()) ? it->second.get() : nullptr;
}

FakeBluetoothMediaTransportClient::Transport*
FakeBluetoothMediaTransportClient::GetTransportByPath(
    const dbus::ObjectPath& transport_path) {
  return GetTransport(GetEndpointPath(transport_path));
}

void FakeBluetoothMediaTransportClient::AcquireInternal(
    bool try_flag,
    const ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  const ObjectPath& endpoint_path = GetEndpointPath(object_path);
  Transport* transport = GetTransport(endpoint_path);
  if (!transport) {
    error_callback.Run(kFailed, "");
    return;
  }

  std::string state = transport->properties->state.value();
  if (state == "active") {
    error_callback.Run(kNotAuthorized, "");
    return;
  }
  if (state != "pending") {
    error_callback.Run(try_flag ? kNotAvailable : kFailed, "");
    return;
  }

  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    transport->input_fd.reset();
    error_callback.Run(kFailed, "");
    return;
  }
  DCHECK(fds[0] > base::kInvalidFd);
  DCHECK(fds[1] > base::kInvalidFd);
  transport->input_fd.reset(new base::File(fds[0]));

  base::ScopedFD out_fd(fds[1]);
  callback.Run(std::move(out_fd), kDefaultReadMtu, kDefaultWriteMtu);
  SetState(endpoint_path, "active");
}

}  // namespace bluez
