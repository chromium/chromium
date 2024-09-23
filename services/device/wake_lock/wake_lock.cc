// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/wake_lock.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "services/device/wake_lock/wake_lock_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/device/wake_lock/wake_lock_context.h"
#include "ui/gfx/native_widget_types.h"
#endif

namespace device {

WakeLock::WakeLock(mojo::PendingReceiver<mojom::WakeLock> receiver,
                   mojom::WakeLockType type,
                   mojom::WakeLockReason reason,
                   const std::string& description,
                   int context_id,
                   WakeLockContextCallback native_view_getter,
                   scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                   Observer* observer)
    : num_lock_requests_(0),
      type_(type),
      reason_(reason),
      description_(std::make_unique<std::string>(description)),
#if BUILDFLAG(IS_ANDROID)
      context_id_(context_id),
      native_view_getter_(native_view_getter),
#endif
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      file_task_runner_(std::move(file_task_runner)),
      observer_(observer) {
  DCHECK(observer_);
  AddClient(std::move(receiver));
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &WakeLock::OnConnectionError, base::Unretained(this)));
}

WakeLock::~WakeLock() {
  // A race condition may cause the WakeLock to be destroyed before it has been
  // removed. In this case, it should still be reset and observers notified.
  if (base::FeatureList::IsEnabled(features::kRemoveWakeLockInDestructor)) {
    if (wake_lock_) {
      RemoveWakeLock();
      CHECK(!wake_lock_);
    }
  }
}

void WakeLock::AddClient(mojo::PendingReceiver<mojom::WakeLock> receiver) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  receiver_set_.Add(this, std::move(receiver), std::make_unique<bool>(false));
}

void WakeLock::RequestWakeLock() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(receiver_set_.current_context());
  DCHECK_GE(num_lock_requests_, 0);

  // Uses the Context to get the outstanding status of current binding.
  // Two consecutive requests from the same client should be coalesced
  // as one request.
  if (*receiver_set_.current_context()) {
    return;
  }

  *receiver_set_.current_context() = true;
  num_lock_requests_++;
  UpdateWakeLock();
}

void WakeLock::CancelWakeLock() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(receiver_set_.current_context());

  // TODO(crbug.com/41443051): Calling CancelWakeLock befoe RequestWakeLock
  // shouldn't be allowed.
  if (!(*receiver_set_.current_context()))
    return;

  DCHECK_GT(num_lock_requests_, 0);
  *receiver_set_.current_context() = false;
  num_lock_requests_--;
  UpdateWakeLock();
}

void WakeLock::ChangeType(mojom::WakeLockType type,
                          ChangeTypeCallback callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

#if BUILDFLAG(IS_ANDROID)
  LOG(ERROR) << "WakeLock::ChangeType() has no effect on Android.";
  std::move(callback).Run(false);
#else
  if (receiver_set_.size() > 1) {
    LOG(ERROR) << "WakeLock::ChangeType() is not allowed when the current wake "
                  "lock is shared by more than one clients.";
    std::move(callback).Run(false);
    return;
  }

  mojom::WakeLockType old_type = type_;
  type_ = type;

  if (type_ != old_type && wake_lock_) {
    SwapWakeLock();
    observer_->OnWakeLockChanged(old_type, type_);
  }

  std::move(callback).Run(true);
#endif
}

void WakeLock::HasWakeLockForTests(HasWakeLockForTestsCallback callback) {
  std::move(callback).Run(!!wake_lock_);
}

void WakeLock::UpdateWakeLock() {
  DCHECK_GE(num_lock_requests_, 0);

  if (num_lock_requests_) {
    if (!wake_lock_)
      CreateWakeLock();
  } else {
    if (wake_lock_)
      RemoveWakeLock();
  }
}

void WakeLock::CreateWakeLock() {
  DCHECK(!wake_lock_);

  wake_lock_ = std::make_unique<PowerSaveBlocker>(
      type_, reason_, *description_, main_task_runner_, file_task_runner_);
  observer_->OnWakeLockActivated(type_);

  if (type_ != mojom::WakeLockType::kPreventDisplaySleep)
    return;

#if BUILDFLAG(IS_ANDROID)
  if (context_id_ == WakeLockContext::WakeLockInvalidContextId) {
    LOG(ERROR) << "Client must pass a valid context_id when requests wake lock "
                  "on Android.";
    return;
  }

  gfx::NativeView native_view = native_view_getter_.Run(context_id_);
  if (native_view)
    wake_lock_.get()->InitDisplaySleepBlocker(native_view);
#endif
}

void WakeLock::RemoveWakeLock() {
  DCHECK(wake_lock_);
  wake_lock_.reset();
  observer_->OnWakeLockDeactivated(type_);
}

void WakeLock::SwapWakeLock() {
  DCHECK(wake_lock_);
  // Do a swap to ensure that there isn't a brief period where the old
  // PowerSaveBlocker is unblocked while the new PowerSaveBlocker is not
  // created.
  auto new_wake_lock = std::make_unique<PowerSaveBlocker>(
      type_, reason_, *description_, main_task_runner_, file_task_runner_);
  wake_lock_.swap(new_wake_lock);
}

void WakeLock::OnConnectionError() {
  // If this client has an outstanding wake lock request, decrease the
  // num_lock_requests and call UpdateWakeLock().
  if (*receiver_set_.current_context() && num_lock_requests_ > 0) {
    num_lock_requests_--;
    UpdateWakeLock();
  }

  if (receiver_set_.empty()) {
    // May delete |this|.
    observer_->OnConnectionError(type_, this);
  }
}

}  // namespace device
