// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_event_watcher.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/stack_container.h"
#include "base/record_replay.h"

// Used to make sure we finish recordings on the main thread, even if we're
// blocked in a sync event.
extern "C" void V8RecordReplayMaybeTerminate(void (*callback)(void*), void* info);

namespace mojo {

SyncEventWatcher::SyncEventWatcher(base::WaitableEvent* event,
                                   base::RepeatingClosure callback)
    : event_(event),
      callback_(std::move(callback)),
      registry_(SyncHandleRegistry::current()),
      destroyed_(new base::RefCountedData<bool>(false)) {}

SyncEventWatcher::~SyncEventWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  destroyed_->data = true;
}

void SyncEventWatcher::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();
}

static void SignalSyncWatcher(void* info) {
  base::RefCountedData<bool>* destroyed = (base::RefCountedData<bool>*) info;
  destroyed->data = true;
}

bool SyncEventWatcher::SyncWatch(const bool** stop_flags,
                                 size_t num_stop_flags) {
  recordreplay::Assert("SyncEventWatcher::SyncWatch Start");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();

  // This object may be destroyed during the Wait() call. So we have to preserve
  // the boolean that Wait uses.
  auto destroyed = destroyed_;

  V8RecordReplayMaybeTerminate(SignalSyncWatcher, destroyed.get());

  constexpr size_t kFlagStackCapacity = 4;
  base::StackVector<const bool*, kFlagStackCapacity> should_stop_array;
  should_stop_array.container().push_back(&destroyed->data);
  std::copy(stop_flags, stop_flags + num_stop_flags,
            std::back_inserter(should_stop_array.container()));
  bool result = registry_->Wait(should_stop_array.container().data(),
                                should_stop_array.container().size());

  V8RecordReplayMaybeTerminate(nullptr, nullptr);

  // This object has been destroyed.
  if (destroyed->data) {
    recordreplay::Assert("SyncEventWatcher::SyncWatch #1");
    return false;
  }

  DecrementRegisterCount();

  recordreplay::Assert("SyncEventWatcher::SyncWatch Done %d", result);
  return result;
}

void SyncEventWatcher::IncrementRegisterCount() {
  if (register_request_count_++ == 0)
    subscription_ = registry_->RegisterEvent(event_, callback_);
}

void SyncEventWatcher::DecrementRegisterCount() {
  DCHECK_GT(register_request_count_, 0u);
  if (--register_request_count_ == 0)
    subscription_.reset();
}

}  // namespace mojo
