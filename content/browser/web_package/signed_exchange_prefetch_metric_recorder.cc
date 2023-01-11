// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"

namespace content {

constexpr size_t kMaxEntries = 256u;

// SignedExchangePrefetchMetricRecorder will not associate prefetch
// to a navigation if they are older than |kPrefetchExpireTimeDelta|.
constexpr base::TimeDelta kPrefetchExpireTimeDelta = base::Milliseconds(30000);
// SignedExchangePrefetchMetricRecorder flushes expired prefetch entries once
// per |kFlushTimeoutTimeDelta|.
constexpr base::TimeDelta kFlushTimeoutTimeDelta = base::Milliseconds(30100);

constexpr char kPrecisionHistogram[] =
    "SignedExchange.Prefetch.Precision.30Seconds";
constexpr char kRecallHistogram[] = "SignedExchange.Prefetch.Recall.30Seconds";

SignedExchangePrefetchMetricRecorder::SignedExchangePrefetchMetricRecorder(
    const base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      flush_timer_(std::make_unique<base::OneShotTimer>(tick_clock)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SignedExchangePrefetchMetricRecorder::~SignedExchangePrefetchMetricRecorder() =
    default;

void SignedExchangePrefetchMetricRecorder::OnSignedExchangePrefetchFinished(
    const GURL& outer_url,
    base::Time response_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_)
    return;

  if (recent_prefetch_entries_.size() > kMaxEntries) {
    // Avoid DoS. Turn off further metric logging to avoid skew too.
    recent_prefetch_entries_.clear();
    disabled_ = true;
    return;
  }

  // Update |prefetch_time| for an existing entry.
  recent_prefetch_entries_.insert_or_assign(
      std::make_pair(outer_url, response_time), tick_clock_->NowTicks());

  if (!flush_timer_->IsRunning())
    ScheduleFlushTimer();
}

void SignedExchangePrefetchMetricRecorder::OnSignedExchangeNonPrefetch(
    const GURL& outer_url,
    base::Time response_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_)
    return;

  auto it =
      recent_prefetch_entries_.find(std::make_pair(outer_url, response_time));
  bool prefetch_found = false;
  if (it != recent_prefetch_entries_.end()) {
    prefetch_found = true;
    recent_prefetch_entries_.erase(it);
  }

  UMA_HISTOGRAM_BOOLEAN(kRecallHistogram, prefetch_found);
  if (prefetch_found)
    UMA_HISTOGRAM_BOOLEAN(kPrecisionHistogram, true);
}

void SignedExchangePrefetchMetricRecorder::ScheduleFlushTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  flush_timer_->Start(
      FROM_HERE, kFlushTimeoutTimeDelta,
      base::BindOnce(&SignedExchangePrefetchMetricRecorder::OnFlushTimer,
                     base::Unretained(this)));
}

void SignedExchangePrefetchMetricRecorder::OnFlushTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_)
    return;

  const base::TimeTicks expire_before =
      tick_clock_->NowTicks() - kPrefetchExpireTimeDelta;

  PrefetchEntries entries;
  std::swap(entries, recent_prefetch_entries_);
  for (const auto& it : entries) {
    const base::TimeTicks prefetch_time = it.second;

    if (prefetch_time < expire_before) {
      UMA_HISTOGRAM_BOOLEAN(kPrecisionHistogram, false);
      continue;
    }

    recent_prefetch_entries_.insert(it);
  }

  if (!recent_prefetch_entries_.empty())
    ScheduleFlushTimer();
}

}  // namespace content
