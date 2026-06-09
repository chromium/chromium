// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/proxy_code_cache_host.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/transaction_error.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/code_cache_util.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"

namespace blink {

#if !BUILDFLAG(IS_FUCHSIA)
class ProxyCodeCacheHost::SourceKeyedCacheReader {
 public:
  SourceKeyedCacheReader() = default;
  ~SourceKeyedCacheReader() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void OnPendingBackend(
      std::optional<persistent_cache::PendingBackend> pending_backend) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(state_, State::kWaitingForCache);

    if (!pending_backend) {
      state_ = State::kInvalid;
      return;
    }
    if (auto result = persistent_cache::PersistentCache::Bind(
            persistent_cache::Client::kCodeCache, *std::move(pending_backend));
        result.has_value()) {
      cache_ = *std::move(result);
      state_ = State::kOpen;
    } else {
      state_ = State::kInvalid;
    }
  }

  void FetchCachedCode(
      base::HeapArray<uint8_t> source_hash,
      base::OnceCallback<void(mojo_base::BigBuffer)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    switch (state_) {
      case State::kWaitingForCache:
      case State::kInvalid:
        std::move(callback).Run({});
        break;

      case State::kOpen: {
        mojo_base::BigBuffer content_buffer;
        auto buffer_provider = [&content_buffer](size_t content_size) {
          content_buffer = mojo_base::BigBuffer(content_size);
          return base::span(content_buffer);
        };

        if (auto metadata_or_error =
                cache_->Find(ComposeSourceKeyedCacheKey(source_hash),
                             std::move(buffer_provider));
            !metadata_or_error.has_value()) {
          switch (metadata_or_error.error()) {
            case persistent_cache::TransactionError::kTransient:
              break;
            case persistent_cache::TransactionError::kConnectionError:
            case persistent_cache::TransactionError::kPermanent:
              cache_.reset();
              state_ = State::kInvalid;
              break;
          }
          std::move(callback).Run({});
        } else if (!metadata_or_error.value()) {
          std::move(callback).Run({});
        } else {
          std::move(callback).Run(std::move(content_buffer));
        }
        break;
      }
    }
  }

 private:
  enum class State {
    kWaitingForCache,
    kInvalid,
    kOpen,
  };

  SEQUENCE_CHECKER(sequence_checker_);
  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kWaitingForCache;
  std::unique_ptr<persistent_cache::PersistentCache> cache_
      GUARDED_BY_CONTEXT(sequence_checker_);
};
#endif  // !BUILDFLAG(IS_FUCHSIA)

ProxyCodeCacheHost::ProxyCodeCacheHost(
    mojo::Remote<mojom::blink::CodeCacheHost> remote)
    : remote_(std::move(remote)) {
  DCHECK(remote_.is_bound());
#if !BUILDFLAG(IS_FUCHSIA)
  if (features::IsInlineScriptCacheEnabled()) {
    reader_ = SequenceBound<SourceKeyedCacheReader>(
        worker_pool::CreateSequencedTaskRunner(base::TaskTraits{
            base::MayBlock(), base::TaskPriority::USER_BLOCKING}));
    remote_->GetPendingBackend(
        mojom::blink::CodeCacheType::kJavascript,
        base::BindOnce(&ProxyCodeCacheHost::OnPendingBackend,
                       weak_factory_.GetWeakPtr()));
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)
}

ProxyCodeCacheHost::~ProxyCodeCacheHost() = default;

mojo_base::BigBuffer ProxyCodeCacheHost::FetchInlineScriptCacheSync(
    const ParkableString& script_source) {
#if !BUILDFLAG(IS_FUCHSIA)
  TRACE_EVENT("loading", "ProxyCodeCacheHost::FetchInlineScriptCacheSync");

  CHECK(reader_);
  return FetchInlineScriptCacheSyncInternal(
      script_source,
      [&reader = reader_](
          base::HeapArray<uint8_t> source_hash,
          base::OnceCallback<void(mojo_base::BigBuffer)> callback) {
        reader.AsyncCall(&SourceKeyedCacheReader::FetchCachedCode)
            .WithArgs(std::move(source_hash), std::move(callback));
      });
#else
  NOTREACHED();
#endif  // !BUILDFLAG(IS_FUCHSIA)
}

base::WeakPtr<CodeCacheHost> ProxyCodeCacheHost::GetWeakPtr() {
  DCHECK(remote_.is_bound());
  return weak_factory_.GetWeakPtr();
}

mojom::blink::CodeCacheHost* ProxyCodeCacheHost::get() {
  return remote_.get();
}

mojom::blink::CodeCacheHost& ProxyCodeCacheHost::operator*() {
  return *remote_.get();
}

mojom::blink::CodeCacheHost* ProxyCodeCacheHost::operator->() {
  return remote_.get();
}

#if !BUILDFLAG(IS_FUCHSIA)
void ProxyCodeCacheHost::OnPendingBackend(
    std::optional<persistent_cache::PendingBackend> pending_backend) {
  CHECK(reader_);
  reader_.AsyncCall(&SourceKeyedCacheReader::OnPendingBackend)
      .WithArgs(std::move(pending_backend));
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace blink
