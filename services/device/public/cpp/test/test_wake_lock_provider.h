// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace device {

// TestWakeLockProvider provides a fake implementation of
// mojom::WakeLockProvider for use in unit tests.
class TestWakeLockProvider : public mojom::WakeLockProvider,
                             public service_manager::Service {
 public:
  TestWakeLockProvider();
  explicit TestWakeLockProvider(service_manager::mojom::ServiceRequest request);
  ~TestWakeLockProvider() override;

  // For internal use only.
  class TestWakeLock;

  void BindReceiver(mojo::PendingReceiver<mojom::WakeLockProvider> receiver);

  // mojom::WakeLockProvider:
  void GetWakeLockContextForID(
      int context_id,
      mojo::PendingReceiver<mojom::WakeLockContext> receiver) override;
  void GetWakeLockWithoutContext(
      mojom::WakeLockType type,
      mojom::WakeLockReason reason,
      const std::string& description,
      mojo::PendingReceiver<mojom::WakeLock> receiver) override;
  void NotifyOnWakeLockDeactivation(
      mojom::WakeLockType type,
      mojo::PendingRemote<mojom::WakeLockObserver> pending_observer) override;
  void GetActiveWakeLocksForTests(
      mojom::WakeLockType type,
      GetActiveWakeLocksForTestsCallback callback) override;

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void OnConnectionError(mojom::WakeLockType type, TestWakeLock* wake_lock);

 private:
  struct WakeLockDataPerType;

  // Returns |WakeLockDataPerType| associated with wake lock of type |type|.
  WakeLockDataPerType& GetWakeLockDataPerType(mojom::WakeLockType type) const;

  // Called by a wake lock when the lock is requested for the first time.
  void OnWakeLockActivated(mojom::WakeLockType type);

  // Called by a wake lock when the lock is canceled for the last time.
  void OnWakeLockDeactivated(mojom::WakeLockType type);

  service_manager::ServiceBinding service_binding_{this};

  mojo::ReceiverSet<mojom::WakeLockProvider> receivers_;

  // Stores wake lock count and observers associated with each wake lock type.
  std::map<mojom::WakeLockType, std::unique_ptr<WakeLockDataPerType>>
      wake_lock_store_;

  DISALLOW_COPY_AND_ASSIGN(TestWakeLockProvider);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_
