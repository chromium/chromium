// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace device {

// TestWakeLockProvider provides a fake implementation of
// mojom::WakeLockProvider for use in unit tests.
class TestWakeLockProvider : public mojom::WakeLockProvider {
 public:
  TestWakeLockProvider();

  TestWakeLockProvider(const TestWakeLockProvider&) = delete;
  TestWakeLockProvider& operator=(const TestWakeLockProvider&) = delete;

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

  void OnConnectionError(mojom::WakeLockType type, TestWakeLock* wake_lock);

 private:
  struct WakeLockDataPerType;

  // Returns |WakeLockDataPerType| associated with wake lock of type |type|.
  WakeLockDataPerType& GetWakeLockDataPerType(mojom::WakeLockType type) const;

  // Called by a wake lock when the lock is requested for the first time.
  void OnWakeLockActivated(mojom::WakeLockType type);

  // Called by a wake lock when the lock is canceled for the last time.
  void OnWakeLockDeactivated(mojom::WakeLockType type);

  mojo::ReceiverSet<mojom::WakeLockProvider> receivers_;

  // Stores wake lock count and observers associated with each wake lock type.
  std::map<mojom::WakeLockType, std::unique_ptr<WakeLockDataPerType>>
      wake_lock_store_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_TEST_WAKE_LOCK_PROVIDER_H_
