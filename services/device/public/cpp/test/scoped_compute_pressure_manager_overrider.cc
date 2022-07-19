// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_compute_pressure_manager_overrider.h"

#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/device_service.h"
#include "services/device/public/mojom/compute_pressure_manager.mojom.h"
#include "services/device/public/mojom/compute_pressure_state.mojom.h"

namespace device {

class ScopedComputePressureManagerOverrider::FakeComputePressureManager
    : public mojom::ComputePressureManager {
 public:
  FakeComputePressureManager();
  ~FakeComputePressureManager() override;

  FakeComputePressureManager(const FakeComputePressureManager&) = delete;
  FakeComputePressureManager& operator=(const FakeComputePressureManager&) =
      delete;

  void Bind(mojo::PendingReceiver<mojom::ComputePressureManager> receiver);

  // mojom::ComputePressureManager implementation.
  void AddClient(mojo::PendingRemote<mojom::ComputePressureClient> client,
                 AddClientCallback callback) override;

  void UpdateClients(const mojom::ComputePressureState& state,
                     base::Time timestamp);

  void set_is_supported(bool is_supported);

 private:
  bool is_supported_ = true;
  mojo::ReceiverSet<mojom::ComputePressureManager> receivers_;
  mojo::RemoteSet<mojom::ComputePressureClient> clients_;
};

ScopedComputePressureManagerOverrider::FakeComputePressureManager::
    FakeComputePressureManager() = default;

ScopedComputePressureManagerOverrider::FakeComputePressureManager::
    ~FakeComputePressureManager() = default;

void ScopedComputePressureManagerOverrider::FakeComputePressureManager::Bind(
    mojo::PendingReceiver<mojom::ComputePressureManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ScopedComputePressureManagerOverrider::FakeComputePressureManager::
    AddClient(mojo::PendingRemote<mojom::ComputePressureClient> client,
              AddClientCallback callback) {
  if (is_supported_) {
    clients_.Add(std::move(client));
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void ScopedComputePressureManagerOverrider::FakeComputePressureManager::
    UpdateClients(const mojom::ComputePressureState& state,
                  base::Time timestamp) {
  for (auto& client : clients_)
    client->ComputePressureStateChanged(state.Clone(), timestamp);
}

void ScopedComputePressureManagerOverrider::FakeComputePressureManager::
    set_is_supported(bool is_supported) {
  is_supported_ = is_supported;
}

ScopedComputePressureManagerOverrider::ScopedComputePressureManagerOverrider() {
  compute_pressure_manager_ = std::make_unique<FakeComputePressureManager>();
  DeviceService::OverrideComputePressureManagerBinderForTesting(
      base::BindRepeating(&FakeComputePressureManager::Bind,
                          base::Unretained(compute_pressure_manager_.get())));
}

ScopedComputePressureManagerOverrider::
    ~ScopedComputePressureManagerOverrider() {
  DeviceService::OverrideComputePressureManagerBinderForTesting(
      base::NullCallback());
}

void ScopedComputePressureManagerOverrider::UpdateClients(
    const mojom::ComputePressureState& state,
    base::Time timestamp) {
  compute_pressure_manager_->UpdateClients(state, timestamp);
}

void ScopedComputePressureManagerOverrider::set_is_supported(
    bool is_supported) {
  compute_pressure_manager_->set_is_supported(is_supported);
}

}  // namespace device
