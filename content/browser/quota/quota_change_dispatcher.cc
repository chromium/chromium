// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_change_dispatcher.h"

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/quota/quota_manager_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// The minimum delay between successive storage pressure events.
constexpr base::TimeDelta kDefaultQuotaChangeIntervalSeconds =
    base::Seconds(60);

}  // namespace

namespace content {

QuotaChangeDispatcher::DelayedStorageKeyListener::DelayedStorageKeyListener()
    : delay(base::RandTimeDeltaUpTo(base::Seconds(2))) {}
QuotaChangeDispatcher::DelayedStorageKeyListener::~DelayedStorageKeyListener() =
    default;

QuotaChangeDispatcher::QuotaChangeDispatcher(
    scoped_refptr<base::SequencedTaskRunner> io_thread)
    : base::RefCountedDeleteOnSequence<QuotaChangeDispatcher>(
          std::move(io_thread)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QuotaChangeDispatcher::~QuotaChangeDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void QuotaChangeDispatcher::MaybeDispatchEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!last_event_dispatched_at_.is_null() &&
      (base::TimeTicks::Now() - last_event_dispatched_at_) <
          GetQuotaChangeEventInterval()) {
    return;
  }
  last_event_dispatched_at_ = base::TimeTicks::Now();

  for (auto& kvp : listeners_by_storage_key_) {
    const blink::StorageKey& storage_key = kvp.first;
    DelayedStorageKeyListener& storage_key_listener = kvp.second;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&QuotaChangeDispatcher::DispatchEventsForStorageKey,
                       weak_ptr_factory_.GetWeakPtr(), storage_key),
        storage_key_listener.delay);
  }
}

void QuotaChangeDispatcher::DispatchEventsForStorageKey(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Handle the case where all the listeners for an storage key were removed
  // during the delay.
  auto it = listeners_by_storage_key_.find(storage_key);
  if (it == listeners_by_storage_key_.end()) {
    return;
  }
  for (auto& listener : it->second.listeners) {
    listener->OnQuotaChange();
  }
}

void QuotaChangeDispatcher::AddChangeListener(
    const blink::StorageKey& storage_key,
    mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_key.origin().opaque()) {
    return;
  }
  // operator[] will default-construct a DelayedStorageKeyListener if
  // `storage_key` does not exist in the map. This serves our needs here.
  DelayedStorageKeyListener* storage_key_listener =
      &(listeners_by_storage_key_[storage_key]);
  storage_key_listener->listeners.Add(std::move(mojo_listener));

  // Using base::Unretained on `storage_key_listener` and
  // `listeners_by_storage_key_` is safe because the lifetime of
  // `storage_key_listener` is tied to the lifetime of
  // `listeners_by_storage_key_` and the lifetime of `listeners_by_storage_key_`
  // is tied to the QuotaChangeDispatcher. This function will be called when the
  // remote is disconnected and at that point the QuotaChangeDispatcher is still
  // alive.
  storage_key_listener->listeners.set_disconnect_handler(
      base::BindRepeating(&QuotaChangeDispatcher::OnRemoteDisconnect,
                          base::Unretained(this), storage_key));
}

void QuotaChangeDispatcher::OnRemoteDisconnect(
    const blink::StorageKey& storage_key,
    mojo::RemoteSetElementId id) {
  DCHECK_GE(listeners_by_storage_key_.count(storage_key), 0U);
  if (listeners_by_storage_key_[storage_key].listeners.empty()) {
    listeners_by_storage_key_.erase(storage_key);
  }
}

const base::TimeDelta QuotaChangeDispatcher::GetQuotaChangeEventInterval() {
  if (!is_quota_change_interval_cached_) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kQuotaChangeEventInterval)) {
      const std::string string_value = command_line->GetSwitchValueASCII(
          switches::kQuotaChangeEventInterval);

      int int_value;
      if (base::StringToInt(string_value, &int_value) && int_value >= 0) {
        return base::Seconds(int_value);
      }
    } else {
      quota_change_event_interval_ = kDefaultQuotaChangeIntervalSeconds;
    }
    is_quota_change_interval_cached_ = true;
  }
  return quota_change_event_interval_;
}

}  // namespace content
