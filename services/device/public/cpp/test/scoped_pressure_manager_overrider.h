// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_

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
  void AddClient(mojo::PendingRemote<mojom::PressureClient> client,
                 AddClientCallback callback) override;

  void UpdateClients(const mojom::PressureUpdate& update);

  void set_is_supported(bool is_supported);

 private:
  bool is_supported_ = true;
  mojo::ReceiverSet<mojom::PressureManager> receivers_;
  mojo::RemoteSet<mojom::PressureClient> clients_;
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
