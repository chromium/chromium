// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/wake_lock_context.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/wake_lock/wake_lock.h"

namespace device {

const int WakeLockContext::WakeLockInvalidContextId = -1;

WakeLockContext::WakeLockContext(
    int context_id,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : file_task_runner_(std::move(file_task_runner)),
      context_id_(context_id),
      native_view_getter_(native_view_getter) {}

WakeLockContext::~WakeLockContext() {}

void WakeLockContext::GetWakeLock(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    mojo::PendingReceiver<mojom::WakeLock> receiver) {
  wake_locks_.push_back(std::make_unique<WakeLock>(
      std::move(receiver), type, reason, description, context_id_,
      native_view_getter_, file_task_runner_, this));
}

void WakeLockContext::OnWakeLockActivated(mojom::WakeLockType type) {}

void WakeLockContext::OnWakeLockDeactivated(mojom::WakeLockType type) {}

void WakeLockContext::OnWakeLockChanged(mojom::WakeLockType old_type,
                                        mojom::WakeLockType new_type) {}

void WakeLockContext::OnConnectionError(mojom::WakeLockType type,
                                        WakeLock* wake_lock) {
  base::EraseIf(wake_locks_,
                [wake_lock](auto& entry) { return entry.get() == wake_lock; });
}

}  // namespace device
