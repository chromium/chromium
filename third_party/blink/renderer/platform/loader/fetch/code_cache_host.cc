// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"

#include <queue>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/transaction_error.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/code_cache_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "url/gurl.h"

namespace blink {

namespace {

// CodeCacheHostImpl -----------------------------------------------------------

// Implementation that delegates to the remote instance. All operations are
// satisfied by `LocalCodeCacheHost` in the browser process. This implementation
// is used when the UsePersistentCacheForCodeCache feature is not enabled.
class CodeCacheHostImpl : public CodeCacheHost {
 public:
  explicit CodeCacheHostImpl(mojo::Remote<mojom::blink::CodeCacheHost> remote)
      : remote_(std::move(remote)) {
    DCHECK(remote_.is_bound());
  }

  // CodeCacheHost:
  base::WeakPtr<CodeCacheHost> GetWeakPtr() override {
    DCHECK(remote_.is_bound());
    return weak_factory_.GetWeakPtr();
  }

  mojom::blink::CodeCacheHost* get() override { return remote_.get(); }
  mojom::blink::CodeCacheHost& operator*() override { return *remote_.get(); }
  mojom::blink::CodeCacheHost* operator->() override { return remote_.get(); }

 private:
  mojo::Remote<mojom::blink::CodeCacheHost> remote_;
  base::WeakPtrFactory<CodeCacheHostImpl> weak_factory_{this};
};

// CodeCacheWithPersistentCacheHostImpl ----------------------------------------

// Implementation that uses a PersistentCache for local fetches on a background
// sequence. Two connections to `CodeCacheWithPersistentCacheHost` instances in
// the browser process are maintained; one for compiled JavaScript, and one for
// compiled WASM. All operations take places on a blocking sequence; calls to
// the browser process (to get a connection to the cache and to insert entries
// into it) and interactions with the cache (to open it and read from it) take
// place on a background sequence.
class CodeCacheWithPersistentCacheHostImpl
    : public CodeCacheHost,
      public mojom::blink::CodeCacheHost {
 public:
  explicit CodeCacheWithPersistentCacheHostImpl(
      mojo::Remote<mojom::blink::CodeCacheHost> remote)
      : async_host_(worker_pool::CreateSequencedTaskRunner(
                        base::TaskTraits{base::MayBlock()}),
                    remote.Unbind()) {}

  // CodeCacheHost:
  base::WeakPtr<::blink::CodeCacheHost> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  mojom::blink::CodeCacheHost* get() override { return this; }
  mojom::blink::CodeCacheHost& operator*() override { return *this; }
  mojom::blink::CodeCacheHost* operator->() override { return this; }

  // mojom::blink::CodeCacheHost:
  void GetPendingBackend(mojom::blink::CodeCacheType cache_type,
                         GetPendingBackendCallback callback) override {
    // This method must never be called on this (the client) end of the
    // connection with the remote CodeCacheHost. This method is for use by this
    // implementation to fetch the data needed to open a local read-only
    // connection to a cache.
    NOTREACHED();
  }

  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const ::blink::KURL& url,
                                    ::base::Time expected_response_time,
                                    ::mojo_base::BigBuffer data) override {
    async_host_.AsyncCall(&AsyncCodeCacheHost::DidGenerateCacheableMetadata)
        .WithArgs(cache_type, url, expected_response_time, std::move(data));
  }

  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const ::blink::KURL& url,
                       FetchCachedCodeCallback callback) override {
    // Handle the reply via a callback bound weakly to `this` to ensure that
    // `callback` is not run after `this` is destroyed.
    async_host_.AsyncCall(&AsyncCodeCacheHost::FetchCachedCode)
        .WithArgs(cache_type, url,
                  base::BindPostTaskToCurrentDefault(
                      ConvertToBaseOnceCallback(CrossThreadBindOnce(
                          &CodeCacheWithPersistentCacheHostImpl::
                              OnFetchCachedCodeReply,
                          weak_factory_.GetWeakPtr(), std::move(callback)))));
  }

  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const ::blink::KURL& url) override {
    // `PersistentCache` does not expose the ability to delete specific entries.
    // This will lead to entries that are known to be unusable (due to
    // response_time mismatches) remaining in the cache. Such unusable entries
    // may be replaced viq new inserts with updated response_times. User-driven
    // requests to clear browsing data will clear caches wholesale rather than
    // delete individual entries.
  }

  void DidGenerateCacheableMetadataInCacheStorage(
      const ::blink::KURL& url,
      ::base::Time expected_response_time,
      ::mojo_base::BigBuffer data,
      const ::blink::String& cache_storage_cache_name) override {
    async_host_
        .AsyncCall(
            &AsyncCodeCacheHost::DidGenerateCacheableMetadataInCacheStorage)
        .WithArgs(url, expected_response_time, std::move(data),
                  cache_storage_cache_name);
  }

 private:
  // The implementation of `CodeCacheHost` that lives on a blocking sequence. It
  // manages a single connection to a `CodeCacheHost` in the browser process and
  // a distinct `PersistentCache` instance (via `RemoteCache`) for each
  // `CodeCacheType`.
  class AsyncCodeCacheHost {
   public:
    explicit AsyncCodeCacheHost(
        mojo::PendingRemote<mojom::blink::CodeCacheHost> pending_remote)
        : remote_(std::move(pending_remote)),
          javascript_cache_(mojom::blink::CodeCacheType::kJavascript,
                            remote_.get()),
          web_assembly_cache_(mojom::blink::CodeCacheType::kWebAssembly,
                              remote_.get()) {
      remote_.set_disconnect_handler(
          BindOnce(&AsyncCodeCacheHost::OnDisconnect, Unretained(this)));
    }

    void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                      const blink::KURL& url,
                                      base::Time expected_response_time,
                                      mojo_base::BigBuffer data) {
      // Inserts are processed on the remote end.
      remote_->DidGenerateCacheableMetadata(
          cache_type, url, expected_response_time, std::move(data));
    }

    void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                         const blink::KURL& url,
                         FetchCachedCodeCallback callback) {
      // Delegate to the appropriate cache.
      switch (cache_type) {
        case mojom::blink::CodeCacheType::kJavascript:
          javascript_cache_.FetchCachedCode(url, std::move(callback));
          break;
        case mojom::blink::CodeCacheType::kWebAssembly:
          web_assembly_cache_.FetchCachedCode(url, std::move(callback));
          break;
      }
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
          url, expected_response_time, std::move(data),
          cache_storage_cache_name);
    }

   private:
    // Handles interactions with a `PersistentCache` for a specific type of
    // data. Connections to a `PersistentCache` are created lazily when the
    // cache is queried. Fetch requests are accumulated while the connection to
    // the cache is being established, and processed as a batch when complete.
    // Queries are processed synchronously when the cache is fully operational.
    class RemoteCache {
     public:
      RemoteCache(mojom::blink::CodeCacheType cache_type,
                  mojom::blink::CodeCacheHostProxy* remote)
          : cache_type_(cache_type), remote_(remote) {}

      ~RemoteCache() {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        InvalidateAndRejectPendingRequests();
      }

      // Fetches a resource from the cache. The cache is lazily-initialized on
      // first use. Fetch requests are accumulated while waiting for
      // initialization and are processed when complete.
      void FetchCachedCode(const ::blink::KURL& url,
                           FetchCachedCodeCallback callback) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        switch (state_) {
          case State::kInitialized:
            // First request since connecting -- fetch backend params.
            remote_->GetPendingBackend(
                cache_type_, ::blink::BindOnce(&RemoteCache::OnPendingBackend,
                                               weak_factory_.GetWeakPtr()));
            state_ = State::kWaitingForCache;
            [[fallthrough]];

          case State::kWaitingForCache:
            // Hold the request until the params arrive.
            requests_.emplace(url, std::move(callback));
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
            if (auto metadata_or_error = cache_->Find(
                    UrlToCodeCacheKey(GURL(url)), std::move(buffer_provider));
                !metadata_or_error.has_value()) {
              switch (metadata_or_error.error()) {
                case persistent_cache::TransactionError::kTransient:
                  // Report this as a cache miss, but keep the cache open.
                  break;
                case persistent_cache::TransactionError::kConnectionError:
                case persistent_cache::TransactionError::kPermanent:
                  // This cache_ instance can no longer be used. Close it and
                  // report a cache miss. Reset back to the `kInitialized` state
                  // so that the next call triggers a new attachment to the
                  // cache.
                  cache_.reset();
                  state_ = State::kInitialized;
                  break;
              }
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

        while (!requests_.empty()) {
          auto& [url, callback] = requests_.front();
          std::move(callback).Run({}, {});
          requests_.pop();
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
        cache_ = persistent_cache::PersistentCache::Bind(
            *std::move(pending_backend));
        if (!cache_) {  // Failed to open the cache.
          InvalidateAndRejectPendingRequests();
          return;
        }

        state_ = State::kOpen;

        // Process all accumulated requests; stopping if a new connection to the
        // cache needs to be established.
        while (!requests_.empty() && state_ != State::kWaitingForCache) {
          auto& [url, callback] = requests_.front();
          FetchCachedCode(url, std::move(callback));
          requests_.pop();
        }
      }

      SEQUENCE_CHECKER(sequence_checker_);
      const mojom::blink::CodeCacheType cache_type_;
      raw_ptr<mojom::blink::CodeCacheHostProxy> remote_
          GUARDED_BY_CONTEXT(sequence_checker_);
      std::unique_ptr<persistent_cache::PersistentCache> cache_;
      std::queue<std::pair<::blink::KURL, FetchCachedCodeCallback>> requests_
          GUARDED_BY_CONTEXT(sequence_checker_);
      State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kInitialized;
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

  void OnFetchCachedCodeReply(FetchCachedCodeCallback callback,
                              base::Time response_time,
                              mojo_base::BigBuffer data) {
    std::move(callback).Run(response_time, std::move(data));
  }

  SequenceBound<AsyncCodeCacheHost> async_host_;
  base::WeakPtrFactory<CodeCacheWithPersistentCacheHostImpl> weak_factory_{
      this};
};

}  // namespace

// CodeCacheHost ---------------------------------------------------------------

// static
std::unique_ptr<CodeCacheHost> CodeCacheHost::Create(
    mojo::Remote<mojom::blink::CodeCacheHost> remote) {
  if (features::IsPersistentCacheForCodeCacheEnabled()) {
    return std::make_unique<CodeCacheWithPersistentCacheHostImpl>(
        std::move(remote));
  }
  return std::make_unique<CodeCacheHostImpl>(std::move(remote));
}

}  // namespace blink
