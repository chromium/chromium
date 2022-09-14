// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/watcher_set.h"

#include <utility>

namespace mojo {
namespace core {

WatcherSet::WatcherSet(Dispatcher* owner) : owner_(owner) {}

WatcherSet::~WatcherSet() = default;

void WatcherSet::NotifyState(const HandleSignalsState& state) {
  // Avoid notifying watchers if they have already seen this state.
  if (last_known_state_.has_value() && state.equals(last_known_state_.value()))
    return;
  last_known_state_ = state;
  for (const auto& entry : watchers_)
    entry.first->NotifyHandleState(owner_, state);
}

void WatcherSet::NotifyClosed() {
  for (const auto& entry : watchers_)
    entry.first->NotifyHandleClosed(owner_);
}

MojoResult WatcherSet::Add(const scoped_refptr<WatcherDispatcher>& watcher,
                           uintptr_t context,
                           const HandleSignalsState& current_state) {
  auto it = watchers_.find(watcher.get());
  if (it == watchers_.end()) {
    auto result =
        watchers_.insert(std::make_pair(watcher.get(), Entry{watcher}));
    it = result.first;
  }

  if (!it->second.contexts.insert(context).second)
    return MOJO_RESULT_ALREADY_EXISTS;

  if (last_known_state_.has_value() &&
      !current_state.equals(last_known_state_.value())) {
    // This new state may be relevant to everyone, in which case we just
    // notify everyone.
    NotifyState(current_state);
  } else {
    // Otherwise only notify the newly added Watcher.
    watcher->NotifyHandleState(owner_, current_state);
  }
  return MOJO_RESULT_OK;
}

MojoResult WatcherSet::Remove(WatcherDispatcher* watcher, uintptr_t context) {
  auto it = watchers_.find(watcher);
  if (it == watchers_.end())
    return MOJO_RESULT_NOT_FOUND;

  ContextSet& contexts = it->second.contexts;
  auto context_it = contexts.find(context);
  if (context_it == contexts.end())
    return MOJO_RESULT_NOT_FOUND;

  contexts.erase(context_it);
  if (contexts.empty())
    watchers_.erase(it);

  return MOJO_RESULT_OK;
}

WatcherSet::Entry::Entry(const scoped_refptr<WatcherDispatcher>& dispatcher)
    : dispatcher(dispatcher) {}

WatcherSet::Entry::Entry(Entry&& other) = default;

WatcherSet::Entry::~Entry() = default;

WatcherSet::Entry& WatcherSet::Entry::operator=(Entry&& other) = default;

}  // namespace core
}  // namespace mojo
