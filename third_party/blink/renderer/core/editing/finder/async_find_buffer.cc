// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/editing/finder/async_find_buffer.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

namespace blink {

namespace {
// Indicates how long FindBuffer task should run before pausing the work.
constexpr base::TimeDelta kFindBufferTaskTimeoutMs =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

void AsyncFindBuffer::FindMatchInRange(Range* search_range,
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

void AsyncFindBuffer::Run(Range* search_range,
                          String search_text,
                          FindOptions options,
                          Callback completeCallback) {
  search_range->OwnerDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);
  EphemeralRangeInFlatTree range = FindBuffer::FindMatchInRange(
      EphemeralRangeInFlatTree(search_range), search_text, options,
      kFindBufferTaskTimeoutMs);

  if (range.IsNotNull() && range.IsCollapsed()) {
    // FindBuffer reached time limit - Start/End of range is last checked
    // position
    search_range->setStart(ToPositionInDOMTree(range.StartPosition()));
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

void AsyncFindBuffer::NextIteration(Range* search_range,
                                    String search_text,
                                    FindOptions options,
                                    Callback completeCallback) {
  iterations_++;
  pending_find_match_task_ = PostCancellableTask(
      *search_range->OwnerDocument()
           .GetTaskRunner(TaskType::kInternalFindInPage)
           .get(),
      FROM_HERE,
      WTF::Bind(&AsyncFindBuffer::Run, WrapWeakPersistent(this),
                WrapWeakPersistent(search_range), search_text, options,
                std::move(completeCallback)));
}

}  // namespace blink
