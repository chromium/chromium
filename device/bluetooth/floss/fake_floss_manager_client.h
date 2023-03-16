// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_MANAGER_CLIENT_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_manager_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossManagerClient
    : public FlossManagerClient {
 public:
  FakeFlossManagerClient();
  ~FakeFlossManagerClient() override;

  // Test utility to do fake notification to observers.
  void NotifyObservers(
      const base::RepeatingCallback<void(Observer*)>& notify) const;

  void SetAdapterPowered(int adapter, bool powered);

  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::OnceClosure on_ready) override;

 private:
  base::WeakPtrFactory<FakeFlossManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_MANAGER_CLIENT_H_
