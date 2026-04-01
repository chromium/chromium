// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_service_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prefetch/pre_prefetch_container.h"
#include "content/browser/preloading/prefetch/pre_prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

namespace {
static network::SharedURLLoaderFactory* g_url_loader_factory_for_testing =
    nullptr;
}  // namespace

// The internal class owned by `PrePrefetchServiceImpl` to run the substantial
// tasks of it on `core_task_runner_` (SequencedTaskRunner) ensuring sequential
// accesses. Please see `PrePrefetchServiceImpl` class comment for more details.
class PrePrefetchServiceCore {
 public:
  PrePrefetchServiceCore(
      base::WeakPtr<BrowserContext> browser_context,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory)
      : browser_context_weak_on_ui_thread_(browser_context),
        factory_(std::move(pending_factory)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  }
  ~PrePrefetchServiceCore() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void StartPrePrefetchRequest(
      base::PassKey<PrePrefetchServiceImpl> pass_key,
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

    std::unique_ptr<const PrefetchRequest> prefetch_request = PrefetchRequest::
        CreateBrowserInitiatedWithoutWebContentsOffTheMainThread(
            browser_context_weak_on_ui_thread_, url,
            PrefetchType(PreloadingTriggerType::kEmbedder,
                         /*use_prefetch_proxy=*/true),
            embedder_histogram_suffix, blink::mojom::Referrer(),
            javascript_enabled,
            /*referring_origin=*/std::nullopt, std::move(no_vary_search_hint),
            priority,
            /*attempt=*/nullptr, additional_headers,
            std::move(request_status_listener), ttl,
            should_append_variations_header,
            should_disable_block_until_head_timeout, should_bypass_http_cache);

    // Handle cases where `factory_` is unexpectedly disconnected, including
    // associated `NetworkContext` crash/restart.
    if (!factory_.is_connected()) {
      // TODO(crbug.com/452389538): Handle this by getting a new factory to the
      // UI thread.
      *out_handle = nullptr;
      return;
    }

    // This `Clone()` doesn't interact with the UI thread and thus isn't
    // blocked by the UI thread. This is because `factory_` represents a
    // `URLLoaderFactory` that is directly connected to the network service, and
    // thus the counterpart of this `Clone()` call is entirely processed by the
    // network service.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> new_factory;
    factory_->Clone(new_factory.InitWithNewPipeAndPassReceiver());

    auto pre_prefetch_container = PrePrefetchContainer::CreateAndStart(
        pass_key, std::move(prefetch_request), std::move(new_factory));

    // ----------------------------------------------------------------------
    // Epilogue
    //
    // This is called via `AsyncCall()` to be run on the `core_task_runner_`.
    // Thus, we set `out_handle` here, which is a substantial return value of
    // this function. `on_done_runner` will automatically signal the event
    // upon destruction.
    *out_handle = std::make_unique<PrePrefetchHandleImpl>(
        std::move(pre_prefetch_container));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // This is UI-thread bound, and must not be dereferenced during this
  // `PrePrefetchServiceCore` sequence.
  base::WeakPtr<BrowserContext> browser_context_weak_on_ui_thread_;

  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
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

  // This is the same default network context that should be used in normal
  // prefetch's `URLLoaderFactory` on the UI thread, created via
  // `PrefetchContainer::GetOrCreateDefaultNetworkContextURLLoaderFactory()`.
  network::mojom::NetworkContext* network_context =
      browser_context->GetDefaultStoragePartition()->GetNetworkContext();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory;
  if (g_url_loader_factory_for_testing) {
    g_url_loader_factory_for_testing->Clone(
        pending_factory.InitWithNewPipeAndPassReceiver());
  } else {
    // This corresponds to `CreatePrefetchURLLoaderFactory()` with
    // `url_loader_factory::HeaderClientOption::kDisallow` and null
    // `url_loader_factory::ContentClientParams`.
    // The interceptors that would be added by `ContentClientParams` will be
    // added/executed when the PrePrefetch is consumed by a `PrefetchContainer`.
    network::URLLoaderFactoryBuilder factory_builder;
    pending_factory =
        std::move(factory_builder)
            .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
                network_context, CreatePrefetchURLLoaderFactoryParams());
  }

  // TODO(crbug.com/452389538): If we have UI-thread dependent variables that
  // will be needed when PrePrefetch, we should have a mechanism here to
  // interact with those. Also, If we need to handle some specific procedure
  // that is tied to the embedder upon PrePrefetch, that should be considered
  // here as well.

  core_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  core_ = base::SequenceBound<PrePrefetchServiceCore>(
      core_task_runner_, browser_context->GetWeakPtr(),
      std::move(pending_factory));
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
      .WithArgs(base::PassKey<PrePrefetchServiceImpl>(), url,
                embedder_histogram_suffix, javascript_enabled,
                std::move(no_vary_search_hint), priority, additional_headers,
                std::move(request_status_listener), ttl,
                should_append_variations_header,
                should_disable_block_until_head_timeout,
                should_bypass_http_cache, &handle, std::move(on_done_runner));

  event.Wait();
  return handle;
}

// static
void PrePrefetchServiceImpl::SetURLLoaderFactoryForTesting(  // IN-TEST
    network::SharedURLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

}  // namespace content
