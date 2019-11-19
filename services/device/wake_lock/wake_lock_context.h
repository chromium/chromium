// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_
#define SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/device/wake_lock/wake_lock.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

// Serves requests for WakeLock connections within a given context.
class WakeLockContext : public mojom::WakeLockContext,
                        public device::WakeLock::Observer {
 public:
  WakeLockContext(int context_id,
                  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                  const WakeLockContextCallback& native_view_getter);
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

  DISALLOW_COPY_AND_ASSIGN(WakeLockContext);
};

}  // namespace device

#endif  // SERVICES_DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_H_
