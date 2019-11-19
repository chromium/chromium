// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/wake_lock_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/wake_lock/wake_lock.h"

namespace device {

// Holds the state associated with wake locks of a single type across the
// system i.e. if 3 |kAppSuspension| wake locks are currently held the |count|
// would be 3.
struct WakeLockProvider::WakeLockDataPerType {
  WakeLockDataPerType() = default;
  ~WakeLockDataPerType() = default;

  // Currently activated wake locks of this wake lock type.
  int64_t count = 0;

  // Map of all wake locks of this type created by this provider. An entry is
  // removed from this map when an |OnConnectionError| is received.
  std::map<WakeLock*, std::unique_ptr<WakeLock>> wake_locks;

  // Observers for this wake lock type.
  mojo::RemoteSet<mojom::WakeLockObserver> observers;

  DISALLOW_COPY_AND_ASSIGN(WakeLockDataPerType);
};

WakeLockProvider::WakeLockProvider(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : file_task_runner_(std::move(file_task_runner)),
      native_view_getter_(native_view_getter) {
  // Populates |wake_lock_store_| with entries for all types of wake locks.
  wake_lock_store_[mojom::WakeLockType::kPreventAppSuspension] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleep] =
      std::make_unique<WakeLockDataPerType>();
  wake_lock_store_[mojom::WakeLockType::kPreventDisplaySleepAllowDimming] =
      std::make_unique<WakeLockDataPerType>();
}

WakeLockProvider::~WakeLockProvider() = default;

void WakeLockProvider::AddBinding(
    mojo::PendingReceiver<mojom::WakeLockProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void WakeLockProvider::GetWakeLockContextForID(
    int context_id,
    mojo::PendingReceiver<mojom::WakeLockContext> receiver) {
  DCHECK_GE(context_id, 0);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WakeLockContext>(context_id, file_task_runner_,
                                        native_view_getter_),
      std::move(receiver));
}

void WakeLockProvider::GetWakeLockWithoutContext(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    mojo::PendingReceiver<mojom::WakeLock> receiver) {
  std::unique_ptr<WakeLock> wake_lock =
      std::make_unique<WakeLock>(std::move(receiver), type, reason, description,
                                 WakeLockContext::WakeLockInvalidContextId,
                                 native_view_getter_, file_task_runner_, this);
  GetWakeLockDataPerType(type).wake_locks[wake_lock.get()] =
      std::move(wake_lock);
}

void WakeLockProvider::NotifyOnWakeLockDeactivation(
    mojom::WakeLockType type,
    mojo::PendingRemote<mojom::WakeLockObserver> pending_observer) {
  mojo::Remote<mojom::WakeLockObserver> observer(std::move(pending_observer));
  // If |type| is not held then notify the observer immediately. Add it to the
  // observer list for future deactivation notifications.
  if (GetWakeLockDataPerType(type).count == 0) {
    observer->OnWakeLockDeactivated(type);
  }
  GetWakeLockDataPerType(type).observers.Add(std::move(observer));
}

void WakeLockProvider::GetActiveWakeLocksForTests(
    mojom::WakeLockType type,
    GetActiveWakeLocksForTestsCallback callback) {
  std::move(callback).Run(GetWakeLockDataPerType(type).count);
}

void WakeLockProvider::OnWakeLockActivated(mojom::WakeLockType type) {
  DVLOG(1) << __func__;
  const int64_t old_count = GetWakeLockDataPerType(type).count;
  DCHECK_GE(old_count, 0);

  GetWakeLockDataPerType(type).count = old_count + 1;
}

void WakeLockProvider::OnWakeLockDeactivated(mojom::WakeLockType type) {
  DVLOG(1) << __func__;
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

void WakeLockProvider::OnWakeLockChanged(mojom::WakeLockType old_type,
                                         mojom::WakeLockType new_type) {
  // This event is only received iff |old_type| had just one client. A change
  // event means there is one less wake lock of |old_type| i.e. the same path
  // as the deactivation event needs to be triggered.
  OnWakeLockDeactivated(old_type);
  OnWakeLockActivated(new_type);
}

void WakeLockProvider::OnConnectionError(mojom::WakeLockType type,
                                         WakeLock* wake_lock) {
  size_t result = GetWakeLockDataPerType(type).wake_locks.erase(wake_lock);
  DCHECK_GT(result, 0UL);
}

WakeLockProvider::WakeLockDataPerType& WakeLockProvider::GetWakeLockDataPerType(
    mojom::WakeLockType type) {
  auto it = wake_lock_store_.find(type);
  // An entry for |type| should always be created in the constructor.
  DCHECK(it != wake_lock_store_.end());
  return *(it->second);
}

}  // namespace device
