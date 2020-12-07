// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota/quota_change_dispatcher.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/quota/quota_manager_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// The minimum delay between successive storage pressure events.
constexpr base::TimeDelta kDefaultQuotaChangeIntervalSeconds =
    base::TimeDelta::FromSeconds(60);

base::TimeDelta GetRandomDelay() {
  int64_t delay_micros = static_cast<int64_t>(
      base::RandInt(0, 2 * base::Time::kMicrosecondsPerSecond));
  return base::TimeDelta::FromMicroseconds(delay_micros);
}

}  // namespace

namespace content {

QuotaChangeDispatcher::DelayedOriginListener::DelayedOriginListener()
    : delay(GetRandomDelay()) {}
QuotaChangeDispatcher::DelayedOriginListener::~DelayedOriginListener() =
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

  for (auto& kvp : listeners_by_origin_) {
    const url::Origin& origin = kvp.first;
    DelayedOriginListener& origin_listener = kvp.second;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&QuotaChangeDispatcher::DispatchEventsForOrigin,
                       weak_ptr_factory_.GetWeakPtr(), origin),
        origin_listener.delay);
  }
}

void QuotaChangeDispatcher::DispatchEventsForOrigin(const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Handle the case where all the listeners for an origin were removed
  // during the delay.
  auto it = listeners_by_origin_.find(origin);
  if (it == listeners_by_origin_.end()) {
    return;
  }
  for (auto& listener : it->second.listeners) {
    listener->OnQuotaChange();
  }
}

void QuotaChangeDispatcher::AddChangeListener(
    const url::Origin& origin,
    mojo::PendingRemote<blink::mojom::QuotaChangeListener> mojo_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (origin.opaque()) {
    return;
  }
  // operator[] will default-construct a DelayedOriginListener if `origin`
  // does not exist in the map. This serves our needs here.
  DelayedOriginListener* origin_listener = &(listeners_by_origin_[origin]);
  origin_listener->listeners.Add(std::move(mojo_listener));

  // Using base::Unretained on `origin_listener` and `listeners_by_origin_`
  // is safe because the lifetime of |origin_listener| is tied to the
  // lifetime of `listeners_by_origin_` and the lifetime of
  // `listeners_by_origin_` is tied to the QuotaChangeDispatcher. This function
  // will be called when the remote is disconnected and at that point the
  // QuotaChangeDispatcher is still alive.
  origin_listener->listeners.set_disconnect_handler(
      base::BindRepeating(&QuotaChangeDispatcher::OnRemoteDisconnect,
                          base::Unretained(this), origin));
}

void QuotaChangeDispatcher::OnRemoteDisconnect(const url::Origin& origin,
                                               mojo::RemoteSetElementId id) {
  DCHECK_GE(listeners_by_origin_.count(origin), 0U);
  if (listeners_by_origin_[origin].listeners.empty()) {
    listeners_by_origin_.erase(origin);
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
        return base::TimeDelta::FromSeconds(int_value);
      }
    } else {
      quota_change_event_interval_ = kDefaultQuotaChangeIntervalSeconds;
    }
    is_quota_change_interval_cached_ = true;
  }
  return quota_change_event_interval_;
}

}  // namespace content
