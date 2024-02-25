// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"

#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/back_forward_cache_utils.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace {

// Maximum number of bytes that can be buffered in total (per-process) by all
// network requests in one renderer process while in back-forward cache.
constexpr size_t kDefaultMaxBufferedBodyBytesPerProcess = 1024 * 1000;

}  // namespace

namespace blink {

BackForwardCacheBufferLimitTracker& BackForwardCacheBufferLimitTracker::Get() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BackForwardCacheBufferLimitTracker, instance,
                                  ());
  return instance;
}

BackForwardCacheBufferLimitTracker::BackForwardCacheBufferLimitTracker()
    : max_buffered_bytes_per_process_(GetLoadingTasksUnfreezableParamAsInt(
          "max_buffered_bytes_per_process",
          kDefaultMaxBufferedBodyBytesPerProcess)) {}

void BackForwardCacheBufferLimitTracker::DidBufferBytes(size_t num_bytes) {
  base::AutoLock lock(lock_);
  total_bytes_buffered_ += num_bytes;
  TRACE_EVENT2("loading", "BackForwardCacheBufferLimitTracker::DidBufferBytes",
               "total_bytes_buffered", static_cast<int>(total_bytes_buffered_),
               "added_bytes", static_cast<int>(num_bytes));
}

void BackForwardCacheBufferLimitTracker::
    DidRemoveFrameOrWorkerFromBackForwardCache(size_t total_bytes) {
  base::AutoLock lock(lock_);
  DCHECK(total_bytes_buffered_ >= total_bytes);
  total_bytes_buffered_ -= total_bytes;
  TRACE_EVENT2("loading",
               "BackForwardCacheBufferLimitTracker::"
               "DidRemoveFrameOrWorkerFromBackForwardCache",
               "total_bytes_buffered", static_cast<int>(total_bytes_buffered_),
               "substracted_bytes", static_cast<int>(total_bytes));
}

bool BackForwardCacheBufferLimitTracker::IsUnderPerProcessBufferLimit() {
  base::AutoLock lock(lock_);
  return total_bytes_buffered_ <= max_buffered_bytes_per_process_;
}

size_t BackForwardCacheBufferLimitTracker::total_bytes_buffered_for_testing() {
  base::AutoLock lock(lock_);
  return total_bytes_buffered_;
}

}  // namespace blink
