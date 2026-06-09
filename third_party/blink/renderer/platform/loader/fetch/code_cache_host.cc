// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/persistent_code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/proxy_code_cache_host.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
std::unique_ptr<CodeCacheHost> CodeCacheHost::Create(
    mojo::Remote<mojom::blink::CodeCacheHost> remote) {
  if (features::IsPersistentCacheForCodeCacheEnabled()) {
    return std::make_unique<PersistentCodeCacheHost>(std::move(remote));
  }
  return std::make_unique<ProxyCodeCacheHost>(std::move(remote));
}

// Helper class to manage synchronous fetch of inline script cache. This class
// is thread-safe. The main thread calls `FetchAsyncAndAwaitForResult()` and
// the callback function `OnFetchCompleted()` is called in a worker thread.
// One fetcher represents one synchronous fetch attempt of inline script
// cache.
class CodeCacheHost::InlineScriptCacheFetcher
    : public ThreadSafeRefCounted<CodeCacheHost::InlineScriptCacheFetcher> {
 public:
  InlineScriptCacheFetcher() = default;
  InlineScriptCacheFetcher(const InlineScriptCacheFetcher&) = delete;
  InlineScriptCacheFetcher& operator=(const InlineScriptCacheFetcher&) = delete;
  InlineScriptCacheFetcher(InlineScriptCacheFetcher&&) = delete;
  InlineScriptCacheFetcher& operator=(InlineScriptCacheFetcher&&) = delete;

  // Starts cache fetch and wait for results by blocking the main thread.
  std::optional<mojo_base::BigBuffer> FetchAsyncAndAwaitForResult(
      base::HeapArray<uint8_t> source_hash,
      CodeCacheHost::InlineScriptCacheFetchTrigger fetch_trigger) {
    base::AutoLock lock(lock_);

    // Starts cache fetch on a worker thread by triggering the callback.
    fetch_trigger(std::move(source_hash),
                  ConvertToBaseOnceCallback(CrossThreadBindOnce(
                      &InlineScriptCacheFetcher::OnFetchCompleted,
                      base::WrapRefCounted(this))));

    // Blocks the main thread to wait for the fetch result.
    base::TimeDelta remaining = features::kInlineScriptCacheTimeout.Get();
    const base::TimeTicks end_time = base::TimeTicks::Now() + remaining;
    do {
      waiter_.TimedWait(remaining);
      if (is_fetch_completed_) {
        // Fetch succeeded.
        return std::move(result_);
      }
      // Spurious wakeup or timeout.
      remaining = end_time - base::TimeTicks::Now();
    } while (remaining.is_positive());
    // Fetch timed out.
    return std::nullopt;
  }

  // Must be called in a worker thread.
  void OnFetchCompleted(mojo_base::BigBuffer data) {
    TRACE_EVENT("loading", "InlineScriptCacheFetcher::OnFetchCompleted");
    base::AutoLock lock(lock_);
    // `OnFetchCompleted()` is called at most once per instance (cache fetch).
    CHECK(!is_fetch_completed_);
    result_ = std::move(data);
    is_fetch_completed_ = true;
    // Note: it is possible that the main thread no longer waits this cache
    // lookup after its timeout or, in theory, crashed. Signaling has no
    // effect in such cases.
    waiter_.Signal();
  }

 private:
  friend class ThreadSafeRefCounted<InlineScriptCacheFetcher>;
  ~InlineScriptCacheFetcher() = default;

  base::Lock lock_;
  // The main thread waits, and a worker thread signal.
  base::ConditionVariable waiter_{&lock_};
  // Read by the main thread, written by a worker thread.
  mojo_base::BigBuffer result_ GUARDED_BY(lock_);
  // This is required to detect spurious wakeups. Note that `result_` can be
  // empty for valid results.
  bool is_fetch_completed_ GUARDED_BY(lock_) = false;
};

// static
mojo_base::BigBuffer CodeCacheHost::FetchInlineScriptCacheSyncInternal(
    const ParkableString& script_source,
    CodeCacheHost::InlineScriptCacheFetchTrigger fetch_trigger) {
  CHECK(features::IsInlineScriptCacheEnabled());

  std::optional<mojo_base::BigBuffer> result;
  InlineScriptCacheFetchResult result_metric;
  if (script_source.length() <
      features::kInlineScriptCacheMinScriptLength.Get()) {
    // Code cache is not produced for small scripts. Skip unnecessary fetch for
    // performance.
    result = mojo_base::BigBuffer{};
    result_metric = InlineScriptCacheFetchResult::kSkippedForSmallScript;
  } else {
    {
      base::ScopedUmaHistogramTimer timer(
          "Blink.Script.InlineScriptCache.FetchTime");
      scoped_refptr fetcher =
          base::MakeRefCounted<CodeCacheHost::InlineScriptCacheFetcher>();
      auto script_hash =
          base::HeapArray<uint8_t>::CopiedFrom(script_source.Digest().Get());
      result = fetcher->FetchAsyncAndAwaitForResult(std::move(script_hash),
                                                    std::move(fetch_trigger));
    }
    if (!result.has_value()) {
      result_metric = InlineScriptCacheFetchResult::kTimedOut;
    } else if (result->size() == 0) {
      result_metric = InlineScriptCacheFetchResult::kFetchedEmpty;
    } else {
      result_metric = InlineScriptCacheFetchResult::kFetchedNonEmpty;
    }
  }
  base::UmaHistogramEnumeration("Blink.Script.InlineScriptCache.FetchResult",
                                result_metric);

  return std::move(result).value_or(mojo_base::BigBuffer{});
}

}  // namespace blink
