// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/persistent_code_cache_host.h"

#include <queue>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/transaction_error.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/code_cache_util.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "url/gurl.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InlineScriptCacheFetchResult)
enum class InlineScriptCacheFetchResult {
  kFetchedNonEmpty = 0,
  kFetchedEmpty = 1,
  kTimedOut = 2,
  kSkippedForSmallScript = 3,
  kMaxValue = kSkippedForSmallScript,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:InlineScriptCacheFetchResult)

}  // namespace

// The implementation of `CodeCacheHost` that lives on a blocking sequence. It
// manages a single connection to a `CodeCacheHost` in the browser process and
// a distinct `PersistentCache` instance for each key/cache type.
class PersistentCodeCacheHost::AsyncCodeCacheHost {
 public:
  explicit AsyncCodeCacheHost(
      mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote)
      : remote_(std::move(pending_remote)),
        javascript_cache_(mojom::blink::CodeCacheType::kJavascript,
                          remote_.get()),
        web_assembly_cache_(mojom::blink::CodeCacheType::kWebAssembly,
                            remote_.get()) {
    remote_.set_disconnect_handler(base::BindOnce(
        &AsyncCodeCacheHost::OnDisconnect, base::Unretained(this)));
  }

  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const blink::KURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) {
    // Inserts are processed on the remote end.
    remote_->DidGenerateCacheableMetadata(
        cache_type, url, expected_response_time, std::move(data));
  }

  void DidGenerateSourceKeyedCacheableMetadata(
      const blink::Vector<uint8_t>& script_hash,
      mojo_base::BigBuffer data) {
    CHECK(features::IsInlineScriptCacheEnabled());
    CHECK_EQ(script_hash.size(), kSha256Bytes);
    // Inserts are processed on the remote end.
    remote_->DidGenerateSourceKeyedCacheableMetadata(script_hash,
                                                     std::move(data));
  }

  void FetchCachedCodeForResource(mojom::blink::CodeCacheType cache_type,
                                  const blink::KURL& url,
                                  FetchCachedCodeCallback callback) {
    // Delegate to the appropriate cache.
    switch (cache_type) {
      case mojom::blink::CodeCacheType::kJavascript:
        javascript_cache_.FetchCachedCodeForResource(url, std::move(callback));
        break;
      case mojom::blink::CodeCacheType::kWebAssembly:
        web_assembly_cache_.FetchCachedCodeForResource(url,
                                                       std::move(callback));
        break;
    }
  }

  void FetchCachedCodeForSourceText(
      base::HeapArray<uint8_t> source_hash,
      base::OnceCallback<void(mojo_base::BigBuffer)> callback) {
    TRACE_EVENT("loading", "AsyncCodeCacheHost::FetchCachedCodeForSourceText");
    javascript_cache_.FetchCachedCodeForSourceText(source_hash,
                                                   std::move(callback));
  }

  void DidGenerateCacheableMetadataInCacheStorage(
      const blink::KURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const blink::String& cache_storage_cache_name) {
    // Inserts are processed on the remote end.

    // TODO(crbug.com/455909145): An fast-follow call to FetchCachedCode for
    // this same `url`'s data will race with the browser process's handling of
    // this call and may consequently fail to see the insert. Consider if/how
    // to address this.
    remote_->DidGenerateCacheableMetadataInCacheStorage(
        url, expected_response_time, std::move(data), cache_storage_cache_name);
  }

 private:
  // Handles interactions with a `PersistentCache` for a specific type of
  // data (JavaScript or WebAssembly). Connections to a `PersistentCache` may
  // be created lazily until the cache is queried. Fetch requests for resource
  // scripts are accumulated while the connection to the cache is being
  // established, and processed as a batch when complete. On the other hand,
  // fetch requests for inline scripts are immediately treated as cache miss
  // before the connection establishment for performance reasons. Queries are
  // processed synchronously when the cache is fully operational.
  class RemoteCache {
   public:
    RemoteCache(mojom::blink::CodeCacheType cache_type,
                mojom::blink::CodeCacheHostProxy* remote)
        : cache_type_(cache_type), remote_(remote) {
      // If the inline script cache is enabled, initiate the connection to
      // the cache backend as soon as possible to mitigate cache miss on the
      // first several cache fetches for inline scripts.
      if (cache_type == mojom::blink::CodeCacheType::kJavascript &&
          features::IsInlineScriptCacheEnabled()) {
        InitiateConnectionToBackend();
      }
    }

    ~RemoteCache() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      InvalidateAndRejectPendingRequests();
    }

    // Fetches an cache entry for a URL-keyed resource. If the connection to
    // the cache is not established, starts connecting and accumulates fetch
    // requests, to be processed when complete.
    void FetchCachedCodeForResource(const KURL& url,
                                    FetchCachedCodeCallback callback) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      switch (state_) {
        case State::kInitialized:
          // First request since connecting -- fetch backend params.
          InitiateConnectionToBackend();
          [[fallthrough]];

        case State::kWaitingForCache:
          // Hold the request until the params arrive.
          pending_requests_.emplace(url, std::move(callback));
          break;

        case State::kInvalid:
          // Cache not operating -- cache miss.
          std::move(callback).Run({}, {});
          break;

        case State::kOpen: {
          mojo_base::BigBuffer content_buffer;

          // A BufferProvider for PersistentCache that puts a new
          // mojo_base::BugBuffer in `content_buffer` to hold an entry's
          // content and returns a view into it.
          auto buffer_provider = [&content_buffer](size_t content_size) {
            content_buffer = mojo_base::BigBuffer(content_size);
            return base::span(content_buffer);
          };

          // Query the PersistentCache.
          const std::string cache_key = UrlToCodeCacheKey(GURL(url));
          if (auto metadata_or_error = cache_->Find(
                  base::as_byte_span(cache_key), std::move(buffer_provider));
              !metadata_or_error.has_value()) {
            HandleTransactionError(metadata_or_error.error());
            std::move(callback).Run({}, {});
          } else if (!metadata_or_error.value()) {
            // Cache miss.
            std::move(callback).Run({}, {});
          } else {
            // Cache hit. The data has been deposited into content_buffer.
            auto& metadata = *metadata_or_error.value();
            std::move(callback).Run(
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(metadata.input_signature)),
                std::move(content_buffer));
          }
          break;
        }
      }
    }

    // Fetches an cache entry for a source-keyed script. If the connection to
    // the cache is not established, starts connecting and treats the current
    // request as cache miss.
    void FetchCachedCodeForSourceText(
        base::span<const uint8_t> source_hash,
        base::OnceCallback<void(mojo_base::BigBuffer)> callback) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

      if (!features::IsInlineScriptCacheEnabled()) {
        std::move(callback).Run({});
        return;
      }

      switch (state_) {
        case State::kInitialized:
          // Connection not established. Try getting connection.
          InitiateConnectionToBackend();
          // Treat as a cache miss.
          std::move(callback).Run({});
          break;

        case State::kWaitingForCache:
          // Treat as a cache miss.
          std::move(callback).Run({});
          break;

        case State::kInvalid:
          // Cache not operating -- cache miss.
          std::move(callback).Run({});
          break;

        case State::kOpen: {
          mojo_base::BigBuffer content_buffer;

          // A BufferProvider for PersistentCache that puts a new
          // mojo_base::BugBuffer in `content_buffer` to hold an entry's
          // content and returns a view into it.
          auto buffer_provider = [&content_buffer](size_t content_size) {
            content_buffer = mojo_base::BigBuffer(content_size);
            return base::span(content_buffer);
          };

          // Query the PersistentCache.
          if (auto metadata_or_error =
                  cache_->Find(ComposeSourceKeyedCacheKey(source_hash),
                               std::move(buffer_provider));
              !metadata_or_error.has_value()) {
            HandleTransactionError(metadata_or_error.error());
            std::move(callback).Run({});
          } else if (!metadata_or_error.value()) {
            // Cache miss.
            std::move(callback).Run({});
          } else {
            // Cache hit. The data has been deposited into `content_buffer`.
            std::move(callback).Run(std::move(content_buffer));
          }
          break;
        }
      }
    }

    // Cleans up the instance upon disconnection from the remote peer.
    void OnDisconnect() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      remote_ = nullptr;
      InvalidateAndRejectPendingRequests();
    }

   private:
    enum class State {
      kInitialized,
      kWaitingForCache,
      kInvalid,
      kOpen,
    };

    // Marks the instance as invalid and rejects all pending requests.
    void InvalidateAndRejectPendingRequests()
        VALID_CONTEXT_REQUIRED(sequence_checker_) {
      cache_.reset();
      state_ = State::kInvalid;
      weak_factory_.InvalidateWeakPtrs();

      while (!pending_requests_.empty()) {
        auto& [url, callback] = pending_requests_.front();
        std::move(callback).Run({}, {});
        pending_requests_.pop();
      }
    }

    // Receives the parameters to connect to the instance's PersistentCache
    // from the remote CodeCacheHost and opens a connection to the cache. All
    // accumulated fetch requests are processed; either by satisfying them
    // and giving them their results, or by rejecting them.
    void OnPendingBackend(
        std::optional<::persistent_cache::PendingBackend> pending_backend) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      CHECK_EQ(state_, State::kWaitingForCache);

      if (!pending_backend) {  // The browser rejected the request.
        InvalidateAndRejectPendingRequests();
        return;
      }
      if (auto result = persistent_cache::PersistentCache::Bind(
              persistent_cache::Client::kCodeCache,
              *std::move(pending_backend));
          result.has_value()) {
        cache_ = *std::move(result);
      } else {
        // Failed to open the cache.
        InvalidateAndRejectPendingRequests();
        return;
      }

      state_ = State::kOpen;

      // Process all accumulated requests; stopping if a new connection to the
      // cache needs to be established.
      while (!pending_requests_.empty() && state_ != State::kWaitingForCache) {
        auto& [url, callback] = pending_requests_.front();
        FetchCachedCodeForResource(url, std::move(callback));
        pending_requests_.pop();
      }
    }

    void InitiateConnectionToBackend()
        VALID_CONTEXT_REQUIRED(sequence_checker_) {
      CHECK_EQ(state_, State::kInitialized);
      remote_->GetPendingBackend(cache_type_,
                                 blink::BindOnce(&RemoteCache::OnPendingBackend,
                                                 weak_factory_.GetWeakPtr()));
      state_ = State::kWaitingForCache;
    }

    void HandleTransactionError(persistent_cache::TransactionError error)
        VALID_CONTEXT_REQUIRED(sequence_checker_) {
      switch (error) {
        case persistent_cache::TransactionError::kTransient:
          // Report this as a cache miss, but keep the cache open.
          return;
        case persistent_cache::TransactionError::kConnectionError:
        case persistent_cache::TransactionError::kPermanent:
          // This cache_ instance can no longer be used. Close it and
          // report a cache miss. Reset back to the `kInitialized` state
          // so that the next call triggers a new attachment to the
          // cache.
          cache_.reset();
          state_ = State::kInitialized;
          return;
      }
    }

    SEQUENCE_CHECKER(sequence_checker_);
    const mojom::blink::CodeCacheType cache_type_;
    raw_ptr<mojom::blink::CodeCacheHostProxy> remote_
        GUARDED_BY_CONTEXT(sequence_checker_);
    State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kInitialized;
    std::unique_ptr<persistent_cache::PersistentCache> cache_;

    // Pending fetch requests for resource scripts while the cache backend is
    // not connected.
    std::queue<std::pair<KURL, FetchCachedCodeCallback>> pending_requests_
        GUARDED_BY_CONTEXT(sequence_checker_);

    base::WeakPtrFactory<RemoteCache> weak_factory_{this};
  };

  void OnDisconnect() {
    web_assembly_cache_.OnDisconnect();
    javascript_cache_.OnDisconnect();
  }

  mojo::Remote<mojom::blink::CodeCacheHost> remote_;
  RemoteCache javascript_cache_;
  RemoteCache web_assembly_cache_;
};

// Helper class to manage synchronous fetch of inline script cache. This class
// is thread-safe. The main thread calls `FetchAsyncAndAwaitForResult()` and
// the callback function `OnFetchCompleted()` is called in a worker thread.
// One fetcher represents one synchronous fetch attempt of inline script
// cache.
class PersistentCodeCacheHost::InlineScriptCacheFetcher
    : public ThreadSafeRefCounted<
          PersistentCodeCacheHost::InlineScriptCacheFetcher> {
 public:
  InlineScriptCacheFetcher() = default;
  InlineScriptCacheFetcher(const InlineScriptCacheFetcher&) = delete;
  InlineScriptCacheFetcher& operator=(const InlineScriptCacheFetcher&) = delete;
  InlineScriptCacheFetcher(InlineScriptCacheFetcher&&) = delete;
  InlineScriptCacheFetcher& operator=(InlineScriptCacheFetcher&&) = delete;

  // Starts cache fetch and wait for results by blocking the main thread.
  std::optional<mojo_base::BigBuffer> FetchAsyncAndAwaitForResult(
      const SequenceBound<PersistentCodeCacheHost::AsyncCodeCacheHost>& host,
      base::HeapArray<uint8_t> source_hash) {
    base::AutoLock lock(lock_);
    // Starts cache fetch on a worker thread.
    host.AsyncCall(&PersistentCodeCacheHost::AsyncCodeCacheHost::
                       FetchCachedCodeForSourceText)
        .WithArgs(std::move(source_hash),
                  ConvertToBaseOnceCallback(CrossThreadBindOnce(
                      &InlineScriptCacheFetcher::OnFetchCompleted,
                      base::WrapRefCounted(this))));
    // Blocks the main thread to wait for the fetch result.
    base::TimeDelta remaining = features::kInlineScriptCacheTimeout.Get();
    const base::TimeTicks end_time = base::TimeTicks::Now() + remaining;
    do {
      waiter_.TimedWait(remaining);
      if (fetch_completed_) {
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
    CHECK(!fetch_completed_);
    result_ = std::move(data);
    fetch_completed_ = true;
    // Note: it is possible that the main thread no longer waits this cache
    // lookup after its timeout or, in theory, crashed. Signaling has no
    // effect in such cases.
    waiter_.Signal();
  }

 private:
  base::Lock lock_;
  // The main thread waits, and a worker thread signal.
  base::ConditionVariable waiter_{&lock_};
  // Read by the main thread, written by a worker thread.
  mojo_base::BigBuffer result_ GUARDED_BY(lock_);
  // This is required to detect spurious wakeups. `result_` can be empty for
  // valid results.
  bool fetch_completed_ GUARDED_BY(lock_) = false;
};

PersistentCodeCacheHost::PersistentCodeCacheHost(
    mojo::Remote<mojom::blink::CodeCacheHost> remote)
    : async_host_(worker_pool::CreateSequencedTaskRunner(
                      base::TaskTraits{base::MayBlock()}),
                  remote.Unbind()) {}

PersistentCodeCacheHost::~PersistentCodeCacheHost() = default;

mojo_base::BigBuffer PersistentCodeCacheHost::FetchInlineScriptCacheSync(
    const ParkableString& script_source) {
  TRACE_EVENT("loading", "PersistentCodeCacheHost::FetchInlineScriptCacheSync");
  CHECK(features::IsInlineScriptCacheEnabled());

  std::optional<mojo_base::BigBuffer> result;
  InlineScriptCacheFetchResult result_metric;
  if (script_source.length() <
      features::kInlineScriptCacheMinScriptLength.Get()) {
    // Code cache is not produced for small scripts. Skip unnecessary fetch
    // for performance.
    result = mojo_base::BigBuffer{};
    result_metric = InlineScriptCacheFetchResult::kSkippedForSmallScript;
  } else {
    {
      base::ScopedUmaHistogramTimer timer(
          "Blink.Script.InlineScriptCache.FetchTime");
      scoped_refptr fetcher = base::MakeRefCounted<InlineScriptCacheFetcher>();
      auto script_hash =
          base::HeapArray<uint8_t>::CopiedFrom(script_source.Digest().Get());
      result = fetcher->FetchAsyncAndAwaitForResult(async_host_,
                                                    std::move(script_hash));
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

base::WeakPtr<::blink::CodeCacheHost> PersistentCodeCacheHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

mojom::blink::CodeCacheHost* PersistentCodeCacheHost::get() {
  return this;
}

mojom::blink::CodeCacheHost& PersistentCodeCacheHost::operator*() {
  return *this;
}

mojom::blink::CodeCacheHost* PersistentCodeCacheHost::operator->() {
  return this;
}

void PersistentCodeCacheHost::GetPendingBackend(
    mojom::blink::CodeCacheType cache_type,
    GetPendingBackendCallback callback) {
  // This method must never be called on this (the client) end of the
  // connection with the remote CodeCacheHost. This method is for use by this
  // implementation to fetch the data needed to open a local read-only
  // connection to a cache.
  NOTREACHED();
}

void PersistentCodeCacheHost::DidGenerateCacheableMetadata(
    mojom::blink::CodeCacheType cache_type,
    const ::blink::KURL& url,
    ::base::Time expected_response_time,
    ::mojo_base::BigBuffer data) {
  async_host_.AsyncCall(&AsyncCodeCacheHost::DidGenerateCacheableMetadata)
      .WithArgs(cache_type, url, expected_response_time, std::move(data));
}

void PersistentCodeCacheHost::DidGenerateSourceKeyedCacheableMetadata(
    const Vector<uint8_t>& script_hash,
    mojo_base::BigBuffer data) {
  async_host_
      .AsyncCall(&AsyncCodeCacheHost::DidGenerateSourceKeyedCacheableMetadata)
      .WithArgs(script_hash, std::move(data));
}

void PersistentCodeCacheHost::FetchCachedCode(
    mojom::blink::CodeCacheType cache_type,
    const ::blink::KURL& url,
    FetchCachedCodeCallback callback) {
  // Handle the reply via a callback bound weakly to `this` to ensure that
  // `callback` is not run after `this` is destroyed.
  async_host_.AsyncCall(&AsyncCodeCacheHost::FetchCachedCodeForResource)
      .WithArgs(cache_type, url,
                base::BindPostTaskToCurrentDefault(
                    ConvertToBaseOnceCallback(CrossThreadBindOnce(
                        &PersistentCodeCacheHost::OnFetchCachedCodeReply,
                        weak_factory_.GetWeakPtr(), std::move(callback)))));
}

void PersistentCodeCacheHost::ClearCodeCacheEntry(
    mojom::blink::CodeCacheType cache_type,
    const ::blink::KURL& url) {
  // `PersistentCache` does not expose the ability to delete specific entries.
  // This will lead to entries that are known to be unusable (due to
  // response_time mismatches) remaining in the cache. Such unusable entries
  // may be replaced via new inserts with updated response_times. User-driven
  // requests to clear browsing data will clear caches wholesale rather than
  // delete individual entries.
}

void PersistentCodeCacheHost::DidGenerateCacheableMetadataInCacheStorage(
    const ::blink::KURL& url,
    ::base::Time expected_response_time,
    ::mojo_base::BigBuffer data,
    const ::blink::String& cache_storage_cache_name) {
  async_host_
      .AsyncCall(
          &AsyncCodeCacheHost::DidGenerateCacheableMetadataInCacheStorage)
      .WithArgs(url, expected_response_time, std::move(data),
                cache_storage_cache_name);
}

void PersistentCodeCacheHost::OnFetchCachedCodeReply(
    FetchCachedCodeCallback callback,
    base::Time response_time,
    mojo_base::BigBuffer data) {
  std::move(callback).Run(response_time, std::move(data));
}

}  // namespace blink
