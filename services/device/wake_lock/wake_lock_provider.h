// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_PROVIDER_H_
#define SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_PROVIDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/device/wake_lock/wake_lock.h"
#include "services/device/wake_lock/wake_lock_context.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

// Serves requests for WakeLockContext connections.
class WakeLockProvider : public mojom::WakeLockProvider,
                         public device::WakeLock::Observer {
 public:
  WakeLockProvider(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                   const WakeLockContextCallback& native_view_getter);
  ~WakeLockProvider() override;

  // Adds this receiver to |receiverss_|.
  void AddBinding(mojo::PendingReceiver<mojom::WakeLockProvider> receiver);

  // mojom::WakeLockProvider overrides.
  void GetWakeLockContextForID(
      int context_id,
      mojo::PendingReceiver<mojom::WakeLockContext> receiver) override;
  void GetWakeLockWithoutContext(
      mojom::WakeLockType type,
      mojom::WakeLockReason reason,
      const std::string& description,
      mojo::PendingReceiver<device::mojom::WakeLock> receiver) override;
  void NotifyOnWakeLockDeactivation(
      mojom::WakeLockType type,
      mojo::PendingRemote<mojom::WakeLockObserver> pending_observer) override;
  void GetActiveWakeLocksForTests(
      mojom::WakeLockType type,
      GetActiveWakeLocksForTestsCallback callback) override;

  // device::WakeLock:Observer overrides.
  void OnWakeLockActivated(mojom::WakeLockType type) override;
  void OnWakeLockDeactivated(mojom::WakeLockType type) override;
  void OnWakeLockChanged(mojom::WakeLockType old_type,
                         mojom::WakeLockType new_type) override;
  void OnConnectionError(mojom::WakeLockType type,
                         WakeLock* wake_lock) override;

 private:
  struct WakeLockDataPerType;

  // Returns |WakeLockDataPerType| associated with wake lock of type |type|.
  WakeLockDataPerType& GetWakeLockDataPerType(mojom::WakeLockType type);

  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  WakeLockContextCallback native_view_getter_;

  mojo::ReceiverSet<mojom::WakeLockProvider> receivers_;

  // Stores wake lock count and observers associated with each wake lock type.
  std::map<mojom::WakeLockType, std::unique_ptr<WakeLockDataPerType>>
      wake_lock_store_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockProvider);
};  // namespace device

}  // namespace device

#endif  // SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_PROVIDER_H_
