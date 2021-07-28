// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

// The adapter manager client observes the Manager interface which lists all
// available adapters and signals when adapters become available + their state
// changes.
//
// Only the manager interface implements ObjectManager so it should be the first
// point of interaction with the platform adapters. It will also be used to
// select the default adapter.
class DEVICE_BLUETOOTH_EXPORT FlossManagerClient : public FlossDBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    Observer() = default;
    ~Observer() override = default;

    virtual void ManagerPresent(bool present) {}
    virtual void AdapterPresent(int adapter, bool present) {}
    virtual void AdapterEnabledChanged(int adapter, bool enabled) {}
  };

  // Convert adapter number to object path.
  static dbus::ObjectPath GenerateAdapterPath(int adapter);

  // Creates the instance.
  static std::unique_ptr<FlossManagerClient> Create();

  FlossManagerClient(const FlossManagerClient&) = delete;
  FlossManagerClient& operator=(const FlossManagerClient&) = delete;

  ~FlossManagerClient() override;

  // Add observers on this client.
  void AddObserver(Observer* observer);

  // Remove observers on this client.
  void RemoveObserver(Observer* observer);

  // Get a list of adapters available on the system.
  virtual std::vector<int> GetAdapters() const = 0;

  // Get the default adapter (index) to use.
  virtual int GetDefaultAdapter() const = 0;

  // Check whether the given adapter is present on the system.
  virtual bool GetAdapterPresent(int adapter) const = 0;

  // Enable or disable Floss at the platform level. This will only be used while
  // Floss is being developed behind a feature flag. This api will be called on
  // the manager to stop Bluez from running and let Floss manage the adapters
  // instead.
  virtual void SetFlossEnabled(bool enable) = 0;

  // Check whether an adapter is enabled.
  virtual bool GetAdapterEnabled(int adapter) const = 0;

  // Enable or disable an adapter.
  virtual void SetAdapterEnabled(int adapter,
                                 bool enabled,
                                 ResponseCallback callback) = 0;

 protected:
  FlossManagerClient();

  // List of observers interested in event notifications from this client.
  base::ObserverList<Observer> observers_;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_MANAGER_CLIENT_H_
