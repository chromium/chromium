// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"
#include <functional>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/preloading//preloading.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

bool CandidatesAreValid(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  for (const auto& candidate : candidates) {
    // These non-http candidates should be filtered out in Blink and
    // SpeculationHostImpl should not see them. If SpeculationHostImpl receives
    // non-http candidates, it may mean the renderer process has a bug
    // or is compromised.
    if (!candidate->url.SchemeIsHTTPOrHTTPS()) {
      mojo::ReportBadMessage("SH_NON_HTTP");
      return false;
    }

    // `target_browsing_context_name_hint` on non-prerender actions should be
    // filtered out in Blink.
    if (candidate->action != blink::mojom::SpeculationAction::kPrerender &&
        candidate->target_browsing_context_name_hint !=
            blink::mojom::SpeculationTargetHint::kNoHint) {
      mojo::ReportBadMessage("SH_TARGET_HINT_ON_PREFETCH");
      return false;
    }
  }
  return true;
}

}  // namespace

// static
void SpeculationHostImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
  CHECK(frame_host);
  // DocumentService will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(*frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : DocumentService(frame_host, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = GetContentClient()->browser()->CreateSpeculationHostDelegate(
      render_frame_host());
}

SpeculationHostImpl::~SpeculationHostImpl() = default;

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CandidatesAreValid(candidates))
    return;

  // Only handle messages from an active main frame.
  if (!render_frame_host().IsActive())
    return;
  if (render_frame_host().GetParent())
    return;

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());

  for (const auto& candidate : candidates) {
    // Create new PreloadingPrediction class and pass all fields for all
    // candidates.

    // In case of speculation rules, the confidence is set as 100 as the URL
    // was not predicted and confidence in this case is not defined.
    int64_t confidence = 100;
    PreloadingURLMatchCallback same_url_matcher =
        PreloadingData::GetSameURLMatcher(candidate->url);

    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents);
    // TODO(crbug.com/1341019): Pass the action requested by speculation rules
    // to PreloadingPrediction.
    preloading_data->AddPreloadingPrediction(
        ToPreloadingPredictor(ContentPreloadingPredictor::kSpeculationRules),
        confidence, std::move(same_url_matcher));
  }

  if (base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor)) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            &render_frame_host());

    prefetch_document_manager->ProcessCandidates(
        candidates, weak_ptr_factory_.GetWeakPtr());
  }

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates, weak_ptr_factory_.GetWeakPtr());

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->UpdateSpeculationCandidates(candidates);
}

void SpeculationHostImpl::OnStartSinglePrefetch(
    const std::string& request_id,
    const network::ResourceRequest& request) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestWillBeSent(
      ftn, request_id, render_frame_host().GetLastCommittedURL(), request);
}

void SpeculationHostImpl::OnPrefetchResponseReceived(
    const GURL& url,
    const std::string& request_id,
    const network::mojom::URLResponseHead& response) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchResponseReceived(ftn, request_id, url,
                                                       response);
}

void SpeculationHostImpl::OnPrefetchRequestComplete(
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestComplete(ftn, request_id, status);
}

void SpeculationHostImpl::OnPrefetchBodyDataReceived(
    const std::string& request_id,
    const std::string& body,
    bool is_base64_encoded) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchBodyDataReceived(ftn, request_id, body,
                                                       is_base64_encoded);
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
SpeculationHostImpl::MakeSelfOwnedNetworkServiceDevToolsObserver() {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  return NetworkServiceDevToolsObserver::MakeSelfOwned(ftn);
}

}  // namespace content
