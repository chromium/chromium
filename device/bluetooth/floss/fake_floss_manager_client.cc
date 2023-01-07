// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_manager_client.h"

#include "base/logging.h"
#include "base/observer_list.h"

namespace floss {

FakeFlossManagerClient::FakeFlossManagerClient() = default;

FakeFlossManagerClient::~FakeFlossManagerClient() = default;

void FakeFlossManagerClient::NotifyObservers(
    const base::RepeatingCallback<void(Observer*)>& notify) const {
  for (auto& observer : observers_) {
    notify.Run(&observer);
  }
}

void FakeFlossManagerClient::SetAdapterPowered(int adapter, bool powered) {
  adapter_to_powered_.emplace(adapter, powered);
}

}  // namespace floss
