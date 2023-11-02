// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_handle_watcher.h"

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"

namespace mojo {

SyncHandleWatcher::SyncHandleWatcher(
    const Handle& handle,
    MojoHandleSignals handle_signals,
    const SyncHandleRegistry::HandleCallback& callback)
    : handle_(handle),
      handle_signals_(handle_signals),
      callback_(callback),
      registered_(false),
      register_request_count_(0),
      registry_(SyncHandleRegistry::current()),
      destroyed_(base::MakeRefCounted<base::RefCountedData<bool>>(false)) {}

SyncHandleWatcher::~SyncHandleWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (registered_)
    registry_->UnregisterHandle(handle_);

  destroyed_->data = true;
}

void SyncHandleWatcher::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();
}

bool SyncHandleWatcher::SyncWatch(const bool* should_stop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();
  if (!registered_) {
    DecrementRegisterCount();
    return false;
  }

  // This object may be destroyed during the Wait() call. So we have to preserve
  // the boolean that Wait uses.
  auto destroyed = destroyed_;
  const bool* should_stop_array[] = {should_stop, &destroyed->data};
  bool result = registry_->Wait(should_stop_array, 2);

  // This object has been destroyed.
  if (destroyed->data)
    return false;

  DecrementRegisterCount();
  return result;
}

void SyncHandleWatcher::IncrementRegisterCount() {
  register_request_count_++;
  if (!registered_) {
    registered_ =
        registry_->RegisterHandle(handle_, handle_signals_, callback_);
  }
}

void SyncHandleWatcher::DecrementRegisterCount() {
  DCHECK_GT(register_request_count_, 0u);

  register_request_count_--;
  if (register_request_count_ == 0 && registered_) {
    registry_->UnregisterHandle(handle_);
    registered_ = false;
  }
}

}  // namespace mojo
