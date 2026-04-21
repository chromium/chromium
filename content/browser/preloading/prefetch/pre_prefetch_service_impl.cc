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
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/pre_prefetch_container.h"
#include "content/browser/preloading/prefetch/pre_prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"
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
      mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory,
      std::map<PrePrefetchPreCalculatedHeadersKey, PrefetchUpdateHeadersParams>
          ui_thread_pre_calculated_headers_map,
      std::vector<PrePrefetchUpdateHeadersCallback>
          non_ui_thread_update_headers_callbacks)
      : factory_(std::move(pending_factory)),
        ui_thread_pre_calculated_headers_map_(
            std::move(ui_thread_pre_calculated_headers_map)),
        non_ui_thread_update_headers_callbacks_(
            std::move(non_ui_thread_update_headers_callbacks)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  }
  ~PrePrefetchServiceCore() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void StartPrePrefetchRequest(
      base::PassKey<PrePrefetchServiceImpl> pass_key,
      std::unique_ptr<const PrefetchRequest> prefetch_request,
      std::unique_ptr<PrePrefetchHandle>* out_handle,
      base::ScopedClosureRunner on_done_runner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(prefetch_request);

    TRACE_EVENT("loading", "PrePrefetchServiceCore::StartPrePrefetchRequest",
                "url", prefetch_request->key().url());

    // Handle cases where `factory_` is unexpectedly disconnected, including
    // associated `NetworkContext` crash/restart.
    if (!factory_.is_connected()) {
      // TODO(crbug.com/452389538): Handle this by getting a new factory to the
      // UI thread.
      *out_handle = nullptr;
      return;
    }

    const PrefetchUpdateHeadersParams* ui_thread_pre_calculated_headers =
        nullptr;
    PrePrefetchPreCalculatedHeadersKey key;
    key.origin = url::Origin::Create(prefetch_request->key().url());
    key.javascript_enabled = prefetch_request->is_javascript_enabled();
    key.should_append_variations_header =
        prefetch_request->should_append_variations_header();
    if (auto it = ui_thread_pre_calculated_headers_map_.find(key);
        it != ui_thread_pre_calculated_headers_map_.end()) {
      ui_thread_pre_calculated_headers = &it->second;
    } else {
      // If we can't find the proper pre-calculated headers with the current
      // request, just make this request fail right now.
      // TODO(crbug.com/452389538): `postTask` to the UI thread to calculate and
      // cache the header for this request.
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
        pass_key, std::move(prefetch_request), std::move(new_factory),
        *ui_thread_pre_calculated_headers,
        non_ui_thread_update_headers_callbacks_);

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

  mojo::Remote<network::mojom::URLLoaderFactory> factory_;

  // Pre-calculated UI-thread headers per
  // `PrePrefetchPreCalculatedHeadersKey`,
  // which is utilized for saving a thread hop to the UI thread if the upcoming
  // PrePrefetch hits this.
  // TODO(crbug.com/452389538): Consider how to refresh these.
  std::map<PrePrefetchPreCalculatedHeadersKey, PrefetchUpdateHeadersParams>
      ui_thread_pre_calculated_headers_map_;

  // Callbacks that can be called on `PrePrefetchContainer::Start()`
  // (`PrePrefetchServiceCore` sequence) to modify the PrePrefetch initial
  // request headers. Therefore, for example, this can conceptually emulate
  // header modifications performed in
  // `ContentBrowserClient::WillCreateURLLoaderFactory`.
  std::vector<PrePrefetchUpdateHeadersCallback>
      non_ui_thread_update_headers_callbacks_;
};

// static
std::unique_ptr<PrePrefetchService> PrePrefetchService::Create(
    BrowserContext* browser_context,
    std::vector<PrePrefetchUpdateHeadersCallback>
        embedder_non_ui_thread_update_headers_callbacks,
    std::optional<url::Origin> initial_origin_hint,
    std::optional<bool> initial_javascript_enabled_hint,
    std::optional<bool> initial_should_append_variations_header_hint) {
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<PrePrefetchServiceImpl>(
      browser_context,
      std::move(embedder_non_ui_thread_update_headers_callbacks),
      initial_origin_hint, initial_javascript_enabled_hint,
      initial_should_append_variations_header_hint);
}

PrePrefetchServiceImpl::PrePrefetchServiceImpl(
    BrowserContext* browser_context,
    std::vector<PrePrefetchUpdateHeadersCallback>
        embedder_non_ui_thread_update_headers_callbacks,
    std::optional<url::Origin> initial_origin_hint,
    std::optional<bool> initial_javascript_enabled_hint,
    std::optional<bool> initial_should_append_variations_header_hint) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "PrePrefetchServiceImpl::PrePrefetchServiceImpl");

  browser_context_weak_on_ui_thread_ = browser_context->GetWeakPtr();

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
    // Unlike `CreatePrefetchURLLoaderFactory()`, this does use
    // `url_loader_factory::HeaderClientOption::kDisallow` and null
    // `url_loader_factory::ContentClientParams`. The interceptors that would be
    // added by `ContentClientParams` will be added/executed when the
    // PrePrefetch is consumed by a `PrefetchContainer`.
    pending_factory = url_loader_factory::CreatePendingRemote(
        ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
        url_loader_factory::TerminalParams::ForNetworkContext(
            network_context, CreatePrefetchURLLoaderFactoryParams(),
            url_loader_factory::HeaderClientOption::kDisallow),
        /*content_client_params=*/std::nullopt);
  }

  // Pre-calculate headers based on the hints. If we can utilize this upon
  // PrePrefetch happening on the UI thread, we can save a thread hop to the UI
  // thread.
  std::map<PrePrefetchPreCalculatedHeadersKey, PrefetchUpdateHeadersParams>
      ui_thread_pre_calculated_headers_map;
  if (initial_origin_hint.has_value() &&
      initial_javascript_enabled_hint.has_value() &&
      initial_should_append_variations_header_hint.has_value()) {
    PrePrefetchPreCalculatedHeadersKey key;
    key.origin = initial_origin_hint.value();
    key.javascript_enabled = initial_javascript_enabled_hint.value();
    key.should_append_variations_header =
        initial_should_append_variations_header_hint.value();
    ui_thread_pre_calculated_headers_map[key] =
        PreCalculatePrePrefetchHeadersOnUI(browser_context, key);
  }

  core_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  core_ = base::SequenceBound<PrePrefetchServiceCore>(
      core_task_runner_, std::move(pending_factory),
      std::move(ui_thread_pre_calculated_headers_map),
      std::move(embedder_non_ui_thread_update_headers_callbacks));
}

PrefetchUpdateHeadersParams
PrePrefetchServiceImpl::PreCalculatePrePrefetchHeadersOnUI(
    BrowserContext* browser_context,
    const PrePrefetchPreCalculatedHeadersKey& key) const {
  // Create a tentative `PrefetchRequest` to pre-calculate headers based on
  // `PrePrefetchPreCalculatedHeadersKey`.
  // TODO(crbug.com/470242977): Creating a full `PrefetchRequest` just to
  // calculate the header is redundant and ambiguous since some members are not
  // affected to the header construction. Once we sort out the general
  // `PrefetchRequest` subset that are required for header construction, we can
  // create and pass it to the utility function instead.
  std::unique_ptr<const PrefetchRequest> tentative_prefetch_request =
      PrefetchRequest::CreateBrowserInitiatedWithoutWebContents(
          browser_context, key.origin.GetURL(),
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/true),
          /*embedder_histogram_suffix=*/"Tentative", blink::mojom::Referrer(),
          key.javascript_enabled,
          /*referring_origin=*/std::nullopt,
          /*no_vary_search_hint=*/std::nullopt,
          /*priority=*/std::nullopt,
          /*preload_pipeline_info=*/
          PreloadPipelineInfo::Create(
              /*planned_max_preloading_type=*/PreloadingType::kPrefetch),
          /*attempt=*/nullptr, /*additional_headers=*/{},
          /*request_status_listener=*/nullptr,
          /*ttl=*/PrefetchContainerDefaultTtlInPrefetchService(),
          /*should_append_variations_header=*/
          key.should_append_variations_header,
          /*should_disable_block_until_head_timeout=*/false);

  // We can safely assume `is_first_party_context_for_variations_header` to be
  // true here because currently PrePrefetches always have no initiator origin.
  // See `variations::IsFirstPartyContext()` for the details.
  // Note: if `should_append_variations_header` is false, variations header
  // will not be created anyway, so this value won't matter.
  // TODO(crbug.com/470242977): Revisit once we set `request_initiator`.
  const bool is_first_party_context_for_variations_header = true;

  return PrepareInitialHeadersForPrefetchPhase2(
      key.origin.GetURL(), *tentative_prefetch_request,
      is_first_party_context_for_variations_header);

  // If we will have additional UI thread dependent headers other than
  // prefetch standard ones above, that should also be considered here,
  // including the one that can come from embedders.
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
  TRACE_EVENT("loading", "PrePrefetchServiceImpl::StartPrePrefetchRequest",
              "url", url);

  std::unique_ptr<const PrefetchRequest> prefetch_request =
      PrefetchRequest::CreateBrowserInitiatedWithoutWebContentsOffTheMainThread(
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

  return StartPrePrefetchRequestInternal(std::move(prefetch_request));
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::StartPrePrefetchRequestForTesting(  // IN-TEST
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  return StartPrePrefetchRequestInternal(std::move(prefetch_request));
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::StartPrePrefetchRequestInternal(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  DCHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(prefetch_request);
  std::unique_ptr<PrePrefetchHandle> handle;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ScopedClosureRunner on_done_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  core_.AsyncCall(&PrePrefetchServiceCore::StartPrePrefetchRequest)
      .WithArgs(base::PassKey<PrePrefetchServiceImpl>(),
                std::move(prefetch_request), &handle,
                std::move(on_done_runner));

  event.Wait();
  return handle;
}

// static
void PrePrefetchServiceImpl::SetURLLoaderFactoryForTesting(  // IN-TEST
    network::SharedURLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

}  // namespace content
