// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"

#include "base/callback_helpers.h"
#include "services/device/device_service.h"

namespace device {

FakePressureManager::FakePressureManager() = default;

FakePressureManager::~FakePressureManager() = default;

void FakePressureManager::Bind(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

bool FakePressureManager::is_bound() const {
  return !receivers_.empty();
}

void FakePressureManager::AddClient(
    mojo::PendingRemote<mojom::PressureClient> client,
    AddClientCallback callback) {
  if (is_supported_) {
    clients_.Add(std::move(client));
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void FakePressureManager::UpdateClients(const mojom::PressureState& state,
                                        base::Time timestamp) {
  for (auto& client : clients_)
    client->PressureStateChanged(state.Clone(), timestamp);
}

void FakePressureManager::set_is_supported(bool is_supported) {
  is_supported_ = is_supported;
}

ScopedPressureManagerOverrider::ScopedPressureManagerOverrider() {
  pressure_manager_ = std::make_unique<FakePressureManager>();
  DeviceService::OverridePressureManagerBinderForTesting(base::BindRepeating(
      &FakePressureManager::Bind, base::Unretained(pressure_manager_.get())));
}

ScopedPressureManagerOverrider::~ScopedPressureManagerOverrider() {
  DeviceService::OverridePressureManagerBinderForTesting(base::NullCallback());
}

void ScopedPressureManagerOverrider::UpdateClients(
    const mojom::PressureState& state,
    base::Time timestamp) {
  pressure_manager_->UpdateClients(state, timestamp);
}

void ScopedPressureManagerOverrider::set_is_supported(bool is_supported) {
  pressure_manager_->set_is_supported(is_supported);
}

void ScopedPressureManagerOverrider::set_fake_pressure_manager(
    std::unique_ptr<FakePressureManager> pressure_manager) {
  DCHECK(!pressure_manager_->is_bound());
  pressure_manager_ = std::move(pressure_manager);
  DeviceService::OverridePressureManagerBinderForTesting(base::BindRepeating(
      &FakePressureManager::Bind, base::Unretained(pressure_manager_.get())));
}

}  // namespace device
