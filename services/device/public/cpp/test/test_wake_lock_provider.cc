// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/test_wake_lock_provider.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace device {

// TestWakeLock implements mojom::WakeLock on behalf of TestWakeLockProvider.
class TestWakeLockProvider::TestWakeLock : public mojom::WakeLock {
 public:
  TestWakeLock(mojo::PendingReceiver<mojom::WakeLock> receiver,
               mojom::WakeLockType type,
               TestWakeLockProvider* provider)
      : type_(type), provider_(provider) {
    AddClient(std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &TestWakeLock::OnConnectionError, base::Unretained(this)));
  }

  TestWakeLock(const TestWakeLock&) = delete;
  TestWakeLock& operator=(const TestWakeLock&) = delete;

  ~TestWakeLock() override = default;

  mojom::WakeLockType type() const { return type_; }

  // mojom::WakeLock:
  void RequestWakeLock() override {
    DCHECK(receivers_.current_context());
    DCHECK_GE(num_lock_requests_, 0);

    // Coalesce consecutive requests from the same client.
    if (*receivers_.current_context())
      return;

    *receivers_.current_context() = true;
    num_lock_requests_++;
    CheckAndNotifyProvider();
  }

  void CancelWakeLock() override {
    DCHECK(receivers_.current_context());

    // Coalesce consecutive cancel requests from the same client. Also ignore a
    // CancelWakeLock call without a RequestWakeLock call.
    if (!(*receivers_.current_context()))
      return;

    DCHECK_GT(num_lock_requests_, 0);
    *receivers_.current_context() = false;
    num_lock_requests_--;
    CheckAndNotifyProvider();
  }

  void AddClient(mojo::PendingReceiver<mojom::WakeLock> receiver) override {
    receivers_.Add(this, std::move(receiver), std::make_unique<bool>(false));
  }

  void ChangeType(mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {
    NOTIMPLEMENTED();
  }

  void OnConnectionError() {
    // If there is an outstanding request by this client then decrement its
    // request and check if the wake lock is deactivated.
    DCHECK(receivers_.current_context());
    if (*receivers_.current_context() && num_lock_requests_ > 0) {
      num_lock_requests_--;
      CheckAndNotifyProvider();
    }

    // TestWakeLockProvider will take care of deleting this object as it owns
    // it.
    if (receivers_.empty())
      provider_->OnConnectionError(type_, this);
  }

 private:
  void CheckAndNotifyProvider() {
    if (num_lock_requests_ == 1) {
      provider_->OnWakeLockActivated(type_);
      return;
    }

    if (num_lock_requests_ == 0) {
      provider_->OnWakeLockDeactivated(type_);
      return;
    }
  }

  mojom::WakeLockType type_;

  // Not owned.
  raw_ptr<TestWakeLockProvider> provider_;

  mojo::ReceiverSet<mojom::WakeLock, std::unique_ptr<bool>> receivers_;

  int num_lock_requests_ = 0;
};

// Holds the state associated with wake locks of a single type across the
// system i.e. if 3 |kAppSuspension| wake locks are currently held the |count|
// would be 3.
struct TestWakeLockProvider::WakeLockDataPerType {
  WakeLockDataPerType() = default;

  WakeLockDataPerType(const WakeLockDataPerType&) = delete;
  WakeLockDataPerType& operator=(const WakeLockDataPerType&) = delete;

  ~WakeLockDataPerType() = default;

  // Currently held count of this wake lock type.
  int64_t count = 0;

  // Map of all wake locks of this type created by this provider. An entry is
  // removed from this map when an |OnConnectionError| is received.
  std::map<TestWakeLock*, std::unique_ptr<TestWakeLock>> wake_locks;

  // Observers for this wake lock type.
  mojo::RemoteSet<mojom::WakeLockObserver> observers;
};

TestWakeLockProvider::TestWakeLockProvider() {
  // Populates |wake_lock_store_| with entries for all types of wake locks.
  wake_lock_store_[mojom::WakeLockType::kPreventAppSuspension] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleep] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleepAllowDimming] =
      std::make_unique<WakeLockDataPerType>();
}

TestWakeLockProvider::~TestWakeLockProvider() = default;

void TestWakeLockProvider::BindReceiver(
    mojo::PendingReceiver<mojom::WakeLockProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TestWakeLockProvider::GetWakeLockContextForID(
    int context_id,
    mojo::PendingReceiver<mojom::WakeLockContext> receiver) {
  // This method is only used on Android.
  NOTIMPLEMENTED();
}

void TestWakeLockProvider::GetWakeLockWithoutContext(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    mojo::PendingReceiver<mojom::WakeLock> receiver) {
  // Create a wake lock and store it to manage it's lifetime.
  auto wake_lock =
      std::make_unique<TestWakeLock>(std::move(receiver), type, this);
  GetWakeLockDataPerType(type).wake_locks[wake_lock.get()] =
      std::move(wake_lock);
}

void TestWakeLockProvider::OnConnectionError(mojom::WakeLockType type,
                                             TestWakeLock* wake_lock) {
  size_t result = GetWakeLockDataPerType(type).wake_locks.erase(wake_lock);
  DCHECK_GT(result, 0UL);
}

TestWakeLockProvider::WakeLockDataPerType&
TestWakeLockProvider::GetWakeLockDataPerType(mojom::WakeLockType type) const {
  auto it = wake_lock_store_.find(type);
  // An entry for |type| should always be created in the constructor.
  CHECK(it != wake_lock_store_.end(), base::NotFatalUntil::M130);
  return *(it->second);
}

void TestWakeLockProvider::OnWakeLockActivated(mojom::WakeLockType type) {
  // Increment the currently activated wake locks of type |type|.
  const int64_t old_count = GetWakeLockDataPerType(type).count;
  DCHECK_GE(old_count, 0);

  GetWakeLockDataPerType(type).count = old_count + 1;
}

void TestWakeLockProvider::OnWakeLockDeactivated(mojom::WakeLockType type) {
  // Decrement the currently activated wake locks of type |type|.
  const int64_t old_count = GetWakeLockDataPerType(type).count;
  DCHECK_GT(old_count, 0);

  const int64_t new_count = old_count - 1;
  GetWakeLockDataPerType(type).count = new_count;
  // Notify observers of the last cancelation i.e. deactivation of wake lock
  // type |type|.
  if (new_count == 0) {
    for (auto& observer : GetWakeLockDataPerType(type).observers)
      observer->OnWakeLockDeactivated(type);
  }
}

void TestWakeLockProvider::NotifyOnWakeLockDeactivation(
    mojom::WakeLockType type,
    mojo::PendingRemote<mojom::WakeLockObserver> pending_observer) {
  mojo::Remote<mojom::WakeLockObserver> observer(std::move(pending_observer));
  // Notify observer immediately if wake lock is deactivated. Add it to the
  // observers list for future deactivation notifications.
  if (GetWakeLockDataPerType(type).count == 0) {
    observer->OnWakeLockDeactivated(type);
  }
  GetWakeLockDataPerType(type).observers.Add(std::move(observer));
}

void TestWakeLockProvider::GetActiveWakeLocksForTests(
    mojom::WakeLockType type,
    GetActiveWakeLocksForTestsCallback callback) {
  std::move(callback).Run(GetWakeLockDataPerType(type).count);
}

}  // namespace device
