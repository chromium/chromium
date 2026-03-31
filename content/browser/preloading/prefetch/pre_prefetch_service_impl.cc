// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_service_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "content/browser/preloading/prefetch/pre_prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

// The internal class owned by `PrePrefetchServiceImpl` to run the substantial
// tasks of it on `core_task_runner_` (SequencedTaskRunner) ensuring sequential
// accesses. Please see `PrePrefetchServiceImpl` class comment for more details.
class PrePrefetchServiceCore {
 public:
  PrePrefetchServiceCore() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  }
  ~PrePrefetchServiceCore() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void StartPrePrefetchRequest(
      const GURL& url,
      const std::string& embedder_histogram_suffix,
      bool javascript_enabled,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      std::optional<PrefetchPriority> priority,
      const net::HttpRequestHeaders& additional_headers,
      std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
      base::TimeDelta ttl,
      bool should_append_variations_header,
      bool should_disable_block_until_head_timeout,
      bool should_bypass_http_cache,
      std::unique_ptr<PrePrefetchHandle>* out_handle,
      base::ScopedClosureRunner on_done_runner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // TODO(crbug.com/452389538): Create an appropriate `PrefetchRequest` based
    // on the given parameters.

    // TODO(crbug.com/452389538): Start PrePrefetch using stored
    // `URLLoaderFactory` etc.

    // This is called via `AsyncCall()` to be run on the `core_task_runner_`.
    // Thus, we set `out_handle` here, which is a substantial return value of
    // this function. `on_done_runner` will automatically signal the event
    // upon destruction.
    *out_handle = std::make_unique<PrePrefetchHandleImpl>();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

// static
std::unique_ptr<PrePrefetchService> PrePrefetchService::Create(
    BrowserContext* browser_context) {
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<PrePrefetchServiceImpl>(browser_context);
}

PrePrefetchServiceImpl::PrePrefetchServiceImpl(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/452389538): Create `URLLoaderFactory` with the given
  // `BrowserContext`'s Default `StoragePartition`'s `NetworkContext`, and store
  // it as pending remote to utilize it later on no UI thread.

  // TODO(crbug.com/452389538): If we have UI-thread dependent variables that
  // will be needed when PrePrefetch, we should have a mechanism here to
  // interact with those. Also, If we need to handle some specific procedure
  // that is tied to the embedder upon PrePrefetch, that should be considered
  // here as well.

  core_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  core_ = base::SequenceBound<PrePrefetchServiceCore>(core_task_runner_);
}

PrePrefetchServiceImpl::~PrePrefetchServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::StartPrePrefetchRequest(
    const GURL& url,
    const std::string& embedder_histogram_suffix,
    bool javascript_enabled,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
    base::TimeDelta ttl,
    bool should_append_variations_header,
    bool should_disable_block_until_head_timeout,
    bool should_bypass_http_cache) {
  DCHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::unique_ptr<PrePrefetchHandle> handle;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ScopedClosureRunner on_done_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  core_.AsyncCall(&PrePrefetchServiceCore::StartPrePrefetchRequest)
      .WithArgs(url, embedder_histogram_suffix, javascript_enabled,
                std::move(no_vary_search_hint), priority, additional_headers,
                std::move(request_status_listener), ttl,
                should_append_variations_header,
                should_disable_block_until_head_timeout,
                should_bypass_http_cache, &handle, std::move(on_done_runner));

  event.Wait();
  return handle;
}

}  // namespace content
