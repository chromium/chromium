// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/pre_prefetch_service_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/pre_prefetch_service_core.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

// static
std::unique_ptr<PrePrefetchService> PrePrefetchService::Create(
    BrowserContext* browser_context,
    std::vector<PrePrefetchUpdateHeadersCallback>
        embedder_non_ui_thread_update_headers_callbacks,
    std::optional<url::Origin> initial_origin_hint,
    std::optional<bool> initial_javascript_enabled_hint,
    std::optional<bool> initial_should_append_variations_header_hint) {
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  base::ElapsedTimer timer;
  if (base::FeatureList::IsEnabled(
          features::kPrePrefetchServiceCreatesPrefetchService)) {
    // Note: This `IsEnabled()` check activates the field trial only for
    // clients calling `PrePrefetchService::Create()`, so that the experiment
    // is evaluated only for them.
    base::ElapsedTimer prefetch_service_timer;
    BrowserContextImpl::From(browser_context)->GetPrefetchService();
    base::UmaHistogramMicrosecondsTimes(
        "Preloading.Prefetch.PrePrefetch.EagerPrefetchServiceCreationTime",
        prefetch_service_timer.Elapsed());
  }
  auto service = std::make_unique<PrePrefetchServiceImpl>(
      browser_context,
      std::move(embedder_non_ui_thread_update_headers_callbacks),
      initial_origin_hint, initial_javascript_enabled_hint,
      initial_should_append_variations_header_hint);
  base::UmaHistogramMicrosecondsTimes(
      "Preloading.Prefetch.PrePrefetch.ServiceCreationTime", timer.Elapsed());
  return service;
}

PrePrefetchServiceImpl::PrePrefetchServiceImpl(
    BrowserContext* browser_context,
    std::vector<PrePrefetchUpdateHeadersCallback>
        embedder_non_ui_thread_update_headers_callbacks,
    std::optional<url::Origin> initial_origin_hint,
    std::optional<bool> initial_javascript_enabled_hint,
    std::optional<bool> initial_should_append_variations_header_hint) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  TRACE_EVENT("loading", "PrePrefetchServiceImpl::PrePrefetchServiceImpl");

  browser_context_weak_on_ui_thread_ = browser_context->GetWeakPtr();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory =
      CreatePrePrefetchURLLoaderFactoryOnUIThread(browser_context);

  // Pre-calculate headers based on the hints. If we can utilize this upon
  // PrePrefetch happening on the UI thread, we can save a thread hop to the UI
  // thread.
  std::map<PrePrefetchPreCalculatedHeadersKey,
           network::HttpRequestHeadersUpdateParams>
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
        PreCalculatePrePrefetchHeadersOnUIThread(browser_context, key);
  }

  core_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  core_ = base::SequenceBound<PrePrefetchServiceCore>(
      core_task_runner_, browser_context->GetWeakPtr(),
      std::move(pending_factory),
      std::move(ui_thread_pre_calculated_headers_map),
      std::move(embedder_non_ui_thread_update_headers_callbacks));
}

PrePrefetchServiceImpl::~PrePrefetchServiceImpl() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::StartPrePrefetchRequest(
    const GURL& url,
    const std::string& histogram_suffix,
    bool javascript_enabled,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
    base::TimeDelta ttl,
    bool should_append_variations_header,
    bool should_disable_block_until_head_timeout,
    bool should_bypass_http_cache) {
  CHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI),
        base::NotFatalUntil::M159);
  TRACE_EVENT("loading", "PrePrefetchServiceImpl::StartPrePrefetchRequest",
              "url", url);

  std::unique_ptr<const PrefetchRequest> prefetch_request =
      PrefetchRequest::CreateBrowserInitiatedWithoutWebContentsOffTheMainThread(
          browser_context_weak_on_ui_thread_, url,
          PrefetchType(PreloadingTriggerType::kEmbedder,
                       /*use_prefetch_proxy=*/false),
          histogram_suffix, blink::mojom::Referrer(), javascript_enabled,
          /*referring_origin=*/std::nullopt, std::move(no_vary_search_hint),
          priority,
          /*attempt=*/nullptr, additional_headers,
          std::move(request_status_listener), ttl,
          should_append_variations_header,
          should_disable_block_until_head_timeout, should_bypass_http_cache);

  return PostStartPrePrefetchRequestAndWait(std::move(prefetch_request));
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::StartPrePrefetchRequestForTesting(  // IN-TEST
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  return PostStartPrePrefetchRequestAndWait(std::move(prefetch_request));
}

std::unique_ptr<PrePrefetchHandle>
PrePrefetchServiceImpl::PostStartPrePrefetchRequestAndWait(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  CHECK(!BrowserThread::CurrentlyOn(content::BrowserThread::UI),
        base::NotFatalUntil::M159);
  CHECK(prefetch_request);
  std::unique_ptr<PrePrefetchHandle> handle;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ScopedClosureRunner on_done_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));

  base::ElapsedTimer timer;
  core_.AsyncCall(&PrePrefetchServiceCore::StartPrePrefetchRequest)
      .WithArgs(base::PassKey<PrePrefetchServiceImpl>(),
                std::move(prefetch_request), &handle,
                std::move(on_done_runner));

  event.Wait();
  UMA_HISTOGRAM_TIMES(
      "Preloading.Prefetch.PrePrefetch.PrePrefetchServiceThreadBlockTime",
      timer.Elapsed());

  return handle;
}

// static
void PrePrefetchServiceImpl::
    SetShouldProhibitURLLoaderFactoryRefreshForTesting(  // IN-TEST
        bool should_prohibit) {
  PrePrefetchServiceCore::
      SetShouldProhibitURLLoaderFactoryRefreshForTesting(  // IN-TEST
          should_prohibit);
}

}  // namespace content
