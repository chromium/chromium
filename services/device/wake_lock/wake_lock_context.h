// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_
#define SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/device/wake_lock/wake_lock.h"

namespace device {

// Serves requests for WakeLock connections within a given context.
class WakeLockContext : public mojom::WakeLockContext,
                        public device::WakeLock::Observer {
 public:
  WakeLockContext(int context_id,
                  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                  const WakeLockContextCallback& native_view_getter);

  WakeLockContext(const WakeLockContext&) = delete;
  WakeLockContext& operator=(const WakeLockContext&) = delete;

  ~WakeLockContext() override;

  // mojom::WakeLockContext:
  void GetWakeLock(mojom::WakeLockType type,
                   mojom::WakeLockReason reason,
                   const std::string& description,
                   mojo::PendingReceiver<mojom::WakeLock> receiver) override;

  static const int WakeLockInvalidContextId;

  // device::WakeLock:Observer overrides.
  void OnWakeLockActivated(mojom::WakeLockType type) override;
  void OnWakeLockDeactivated(mojom::WakeLockType type) override;
  void OnWakeLockChanged(mojom::WakeLockType old_type,
                         mojom::WakeLockType new_type) override;
  void OnConnectionError(mojom::WakeLockType type,
                         WakeLock* wake_lock) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  int context_id_;
  WakeLockContextCallback native_view_getter_;

  // All wake locks acquired via this class are managed here.
  std::vector<std::unique_ptr<WakeLock>> wake_locks_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_
