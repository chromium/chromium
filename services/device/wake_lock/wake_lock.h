// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_H_
#define SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

// Callback that maps a context ID to the NativeView associated with
// that context. This callback is provided to the Device Service by its
// embedder.
using WakeLockContextCallback = base::RepeatingCallback<gfx::NativeView(int)>;

class WakeLock : public mojom::WakeLock {
 public:
  // This is an interface for classes to be notified of wake lock events.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the tracked wake lock is requested the first time i.e. the
    // number of holders increases to 1.
    virtual void OnWakeLockActivated(mojom::WakeLockType type) {}

    // Called when the tracked wake lock is canceled the last time i.e. the
    // number of holders goes to 0.
    virtual void OnWakeLockDeactivated(mojom::WakeLockType type) {}

    // Called when the tracked wake lock's type is changed via ChangeType.
    // |old_type| refers to its old type and new type refers to its new type.
    virtual void OnWakeLockChanged(mojom::WakeLockType old_type,
                                   mojom::WakeLockType new_type) {}

    // Called when |WakeLock| has no bindings left.
    virtual void OnConnectionError(mojom::WakeLockType type,
                                   WakeLock* wake_lock) {}
  };

  // |observer| must outlive this WakeLock instance.
  WakeLock(mojo::PendingReceiver<mojom::WakeLock> receiver,
           mojom::WakeLockType type,
           mojom::WakeLockReason reason,
           const std::string& description,
           int context_id,
           WakeLockContextCallback native_view_getter,
           scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
           Observer* observer);

  WakeLock(const WakeLock&) = delete;
  WakeLock& operator=(const WakeLock&) = delete;

  ~WakeLock() override;

  // mojom::WakeLock implementation.
  void RequestWakeLock() override;
  void CancelWakeLock() override;
  void AddClient(mojo::PendingReceiver<mojom::WakeLock> receiver) override;
  void ChangeType(mojom::WakeLockType type,
                  ChangeTypeCallback callback) override;
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override;

 protected:
  int num_lock_requests_;

 private:
  virtual void UpdateWakeLock();
  virtual void CreateWakeLock();
  virtual void RemoveWakeLock();
  virtual void SwapWakeLock();

  void OnConnectionError();

  mojom::WakeLockType type_;
  mojom::WakeLockReason reason_;
  std::unique_ptr<std::string> description_;

#if BUILDFLAG(IS_ANDROID)
  int context_id_;
  WakeLockContextCallback native_view_getter_;
#endif

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // The actual power save blocker for screen.
  std::unique_ptr<PowerSaveBlocker> wake_lock_;

  // Not owned. |observer_| must outlive this instance of WakeLock.
  const raw_ptr<Observer> observer_;

  // Multiple clients that associate to the same WebContents share the same one
  // WakeLock instance. Two consecutive |RequestWakeLock| requests
  // from the same client should be coalesced as one request. Everytime a new
  // client is being added into the ReceiverSet, we create an unique_ptr<bool>
  // as its context, which records this client's request status.
  mojo::ReceiverSet<mojom::WakeLock, std::unique_ptr<bool>> receiver_set_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_H_
