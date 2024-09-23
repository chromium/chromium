// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_

#include <map>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace device {

class FakePressureManager : public mojom::PressureManager {
 public:
  FakePressureManager();
  ~FakePressureManager() override;

  FakePressureManager(const FakePressureManager&) = delete;
  FakePressureManager& operator=(const FakePressureManager&) = delete;

  void Bind(mojo::PendingReceiver<mojom::PressureManager> receiver);
  bool is_bound() const;

  // mojom::PressureManager implementation.
  void AddClient(mojom::PressureSource source,
                 const std::optional<base::UnguessableToken>& token,
                 AddClientCallback callback) override;

  void UpdateClients(const mojom::PressureUpdate& update);

  void set_is_supported(bool is_supported);

 private:
  void AddVirtualPressureSource(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      mojom::VirtualPressureSourceMetadataPtr metadata,
      AddVirtualPressureSourceCallback callback) override {}
  void RemoveVirtualPressureSource(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      RemoveVirtualPressureSourceCallback callback) override {}
  void UpdateVirtualPressureSourceState(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      mojom::PressureState state,
      UpdateVirtualPressureSourceStateCallback callback) override {}

  bool is_supported_ = true;
  mojo::ReceiverSet<mojom::PressureManager> receivers_;
  std::map<mojom::PressureSource, mojo::RemoteSet<mojom::PressureClient>>
      clients_;
};

class ScopedPressureManagerOverrider {
 public:
  ScopedPressureManagerOverrider();
  ~ScopedPressureManagerOverrider();

  ScopedPressureManagerOverrider(const ScopedPressureManagerOverrider&) =
      delete;
  ScopedPressureManagerOverrider& operator=(
      const ScopedPressureManagerOverrider&) = delete;

  void UpdateClients(const mojom::PressureUpdate& update);

  void set_is_supported(bool is_supported);

  void set_fake_pressure_manager(
      std::unique_ptr<FakePressureManager> pressure_manager);

 private:
  std::unique_ptr<FakePressureManager> pressure_manager_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_
