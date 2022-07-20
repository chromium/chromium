// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"

#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/device_service.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_state.mojom.h"

namespace device {

class ScopedPressureManagerOverrider::FakePressureManager
    : public mojom::PressureManager {
 public:
  FakePressureManager();
  ~FakePressureManager() override;

  FakePressureManager(const FakePressureManager&) = delete;
  FakePressureManager& operator=(const FakePressureManager&) = delete;

  void Bind(mojo::PendingReceiver<mojom::PressureManager> receiver);

  // mojom::PressureManager implementation.
  void AddClient(mojo::PendingRemote<mojom::PressureClient> client,
                 AddClientCallback callback) override;

  void UpdateClients(const mojom::PressureState& state, base::Time timestamp);

  void set_is_supported(bool is_supported);

 private:
  bool is_supported_ = true;
  mojo::ReceiverSet<mojom::PressureManager> receivers_;
  mojo::RemoteSet<mojom::PressureClient> clients_;
};

ScopedPressureManagerOverrider::FakePressureManager::FakePressureManager() =
    default;

ScopedPressureManagerOverrider::FakePressureManager::~FakePressureManager() =
    default;

void ScopedPressureManagerOverrider::FakePressureManager::Bind(
    mojo::PendingReceiver<mojom::PressureManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ScopedPressureManagerOverrider::FakePressureManager::AddClient(
    mojo::PendingRemote<mojom::PressureClient> client,
    AddClientCallback callback) {
  if (is_supported_) {
    clients_.Add(std::move(client));
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void ScopedPressureManagerOverrider::FakePressureManager::UpdateClients(
    const mojom::PressureState& state,
    base::Time timestamp) {
  for (auto& client : clients_)
    client->PressureStateChanged(state.Clone(), timestamp);
}

void ScopedPressureManagerOverrider::FakePressureManager::set_is_supported(
    bool is_supported) {
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

}  // namespace device
