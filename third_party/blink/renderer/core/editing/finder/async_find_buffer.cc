// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/editing/finder/async_find_buffer.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

namespace blink {

namespace {
// Indicates how long FindBuffer task should run before pausing the work.
constexpr base::TimeDelta kFindBufferTaskTimeout = base::Milliseconds(100);

// Global static to allow tests to override the timeout.
base::TimeDelta g_find_buffer_timeout = kFindBufferTaskTimeout;
}  // namespace

// static
std::unique_ptr<base::AutoReset<base::TimeDelta>>
AsyncFindBuffer::OverrideTimeoutForTesting(base::TimeDelta timeout_override) {
  return std::make_unique<base::AutoReset<base::TimeDelta>>(
      &g_find_buffer_timeout, timeout_override);
}

void AsyncFindBuffer::FindMatchInRange(RangeInFlatTree* search_range,
                                       String search_text,
                                       FindOptions options,
                                       Callback completeCallback) {
  iterations_ = 0;
  search_start_time_ = base::TimeTicks::Now();
  NextIteration(search_range, search_text, options,
                std::move(completeCallback));
}

void AsyncFindBuffer::Cancel() {
  pending_find_match_task_.Cancel();
}

void AsyncFindBuffer::Run(RangeInFlatTree* search_range,
                          String search_text,
                          FindOptions options,
                          Callback completeCallback) {
  // If range is not connected we should stop the search.
  if (search_range->IsNull() || !search_range->IsConnected()) {
    std::move(completeCallback).Run(EphemeralRangeInFlatTree());
    return;
  }
  search_range->StartPosition().GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);

  EphemeralRangeInFlatTree range =
      FindBuffer::FindMatchInRange(search_range->ToEphemeralRange(),
                                   search_text, options, g_find_buffer_timeout);

  if (range.IsNotNull() && range.IsCollapsed()) {
    // FindBuffer reached time limit - Start/End of range is last checked
    // position
    search_range->SetStart(range.StartPosition());
    NextIteration(search_range, search_text, options,
                  std::move(completeCallback));
    return;
  }

  // Search finished, return the result
  UMA_HISTOGRAM_COUNTS_100("SharedHighlights.AsyncTask.Iterations",
                           iterations_);
  UMA_HISTOGRAM_TIMES("SharedHighlights.AsyncTask.SearchDuration",
                      base::TimeTicks::Now() - search_start_time_);

  std::move(completeCallback).Run(range);
}

void AsyncFindBuffer::NextIteration(RangeInFlatTree* search_range,
                                    String search_text,
                                    FindOptions options,
                                    Callback completeCallback) {
  iterations_++;
  pending_find_match_task_ = PostCancellableTask(
      *search_range->StartPosition()
           .GetDocument()
           ->GetTaskRunner(TaskType::kInternalFindInPage)
           .get(),
      FROM_HERE,
      WTF::BindOnce(&AsyncFindBuffer::Run, WrapWeakPersistent(this),
                    WrapWeakPersistent(search_range), search_text, options,
                    std::move(completeCallback)));
}

}  // namespace blink
