// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothProfileManagerClient is used to communicate with the profile
// manager object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothProfileManagerClient
    : public BluezDBusClient {
 public:
  // Species the role of the object within the profile. SYMMETRIC should be
  // usually used unless the profile requires you specify as a CLIENT or as a
  // SERVER.
  enum ProfileRole { SYMMETRIC, CLIENT, SERVER };

  // Options used to register a Profile object.
  struct DEVICE_BLUETOOTH_EXPORT Options {
    Options();
    ~Options();

    // Human readable name for the profile.
    std::unique_ptr<std::string> name;

    // Primary service class UUID (if different from the actual UUID)
    std::unique_ptr<std::string> service;

    // Role.
    enum ProfileRole role = ProfileRole::SYMMETRIC;

    // RFCOMM channel number.
    std::unique_ptr<uint16_t> channel;

    // PSM number.
    std::unique_ptr<uint16_t> psm;

    // Pairing is required before connections will be established.
    std::unique_ptr<bool> require_authentication;

    // Request authorization before connections will be established.
    std::unique_ptr<bool> require_authorization;

    // Force connections when a remote device is connected.
    std::unique_ptr<bool> auto_connect;

    // Manual SDP record.
    std::unique_ptr<std::string> service_record;

    // Profile version.
    std::unique_ptr<uint16_t> version;

    // Profile features.
    std::unique_ptr<uint16_t> features;
  };

  BluetoothProfileManagerClient(const BluetoothProfileManagerClient&) = delete;
  BluetoothProfileManagerClient& operator=(
      const BluetoothProfileManagerClient&) = delete;

  ~BluetoothProfileManagerClient() override;

  // The ErrorCallback is used by adapter methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Registers a profile implementation within the local process at the
  // D-bus object path |profile_path| with the remote profile manager.
  // |uuid| specifies the identifier of the profile and |options| the way in
  // which the profile is implemented.
  virtual void RegisterProfile(const dbus::ObjectPath& profile_path,
                               const std::string& uuid,
                               const Options& options,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) = 0;

  // Unregisters the profile with the D-Bus object path |agent_path| from the
  // remote profile manager.
  virtual void UnregisterProfile(const dbus::ObjectPath& profile_path,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothProfileManagerClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  BluetoothProfileManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
