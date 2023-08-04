// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_manager_client.h"

#include "base/logging.h"
#include "base/observer_list.h"

namespace floss {

FakeFlossManagerClient::FakeFlossManagerClient() = default;

FakeFlossManagerClient::~FakeFlossManagerClient() = default;

void FakeFlossManagerClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index,
                                  base::OnceClosure on_ready) {
  std::move(on_ready).Run();
}

void FakeFlossManagerClient::NotifyObservers(
    const base::RepeatingCallback<void(Observer*)>& notify) const {
  for (auto& observer : observers_) {
    notify.Run(&observer);
  }
}

void FakeFlossManagerClient::SetDefaultEnabled(bool enabled) {
  adapter_to_enabled_.insert_or_assign(GetDefaultAdapter(), enabled);
}

}  // namespace floss
