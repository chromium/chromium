// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

PrerenderNewTabHandle::PrerenderNewTabHandle(
    const PrerenderAttributes& attributes,
    BrowserContext& browser_context)
    : attributes_(attributes), web_contents_create_params_(&browser_context) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab));
  CHECK(!attributes.IsBrowserInitiated());

  auto* initiator_render_frame_host = RenderFrameHostImpl::FromFrameToken(
      attributes_.initiator_process_id,
      attributes_.initiator_frame_token.value());

  // Create a new WebContents for prerendering in a new tab.
  // TODO(crbug.com/40234240): Pass the same creation parameters as
  // WebContentsImpl::CreateNewWindow().
  web_contents_create_params_.opener_render_process_id =
      initiator_render_frame_host->GetProcess()->GetID();
  web_contents_create_params_.opener_render_frame_id =
      initiator_render_frame_host->GetRoutingID();
  web_contents_create_params_.opener_suppressed = true;

  // Set the visibility of the prerendering WebContents to HIDDEN until
  // prerender activation.
  web_contents_create_params_.initially_hidden = true;

  // TODO(crbug.com/40234240): Consider sharing a pre-created WebContents
  // instance among multiple new-tab-prerenders as an optimization.
  web_contents_ = base::WrapUnique(static_cast<WebContentsImpl*>(
      WebContents::Create(web_contents_create_params_).release()));

  // The delegate is swapped with a proper one on activation.
  web_contents_delegate_ =
      GetContentClient()->browser()->CreatePrerenderWebContentsDelegate();
  web_contents_delegate_->PrerenderWebContentsCreated(web_contents_.get());
  web_contents_->SetDelegate(web_contents_delegate_.get());

  // The prerendering WebContents is not visible until activation but should
  // have a valid initial empty primary page. This condition is important as
  // WebContentsObservers attached to the prerendering WebContents may assume
  // there is the primary page and access it during prerendering.
  CHECK_EQ(web_contents_->GetVisibility(), Visibility::HIDDEN);
  CHECK(web_contents_->GetPrimaryMainFrame());
  CHECK(web_contents_->GetPrimaryMainFrame()->is_initial_empty_document());
}

PrerenderNewTabHandle::~PrerenderNewTabHandle() {
  if (web_contents_)
    web_contents_->SetDelegate(nullptr);
}

FrameTreeNodeId PrerenderNewTabHandle::StartPrerendering(
    const PreloadingPredictor& creating_predictor,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) {
  CHECK(web_contents_);
  CHECK(attributes_.initiator_web_contents);

  // Create new PreloadingAttempt and pass all the values corresponding to
  // this prerendering attempt.
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents_.get());
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(attributes_.prerendering_url);
  ukm::SourceId triggered_primary_page_source_id =
      attributes_.initiator_web_contents->GetPrimaryMainFrame()
          ->GetPageUkmSourceId();
  auto* preloading_attempt =
      static_cast<PreloadingAttemptImpl*>(preloading_data->AddPreloadingAttempt(
          creating_predictor, enacting_predictor, PreloadingType::kPrerender,
          std::move(same_url_matcher),
          /*planned_max_preloading_type=*/std::nullopt,
          triggered_primary_page_source_id));
  preloading_data->AddPreloadingPrediction(
      enacting_predictor, confidence,
      PreloadingData::GetSameURLMatcher(attributes_.prerendering_url),
      triggered_primary_page_source_id);
  preloading_data->CopyPredictorDomains(
      *PreloadingDataImpl::GetOrCreateForWebContents(
          attributes_.initiator_web_contents.get()),
      {creating_predictor, enacting_predictor});
  CHECK(attributes_.eagerness.has_value());
  preloading_attempt->SetSpeculationEagerness(attributes_.eagerness.value());

  prerender_host_id_ = GetPrerenderHostRegistry().CreateAndStartHost(
      attributes_, preloading_attempt);
  return prerender_host_id_;
}

void PrerenderNewTabHandle::CancelPrerendering(
    const PrerenderCancellationReason& reason) {
  GetPrerenderHostRegistry().CancelHost(prerender_host_id_, reason);
}

std::unique_ptr<WebContentsImpl>
PrerenderNewTabHandle::TakeWebContentsIfAvailable(
    const mojom::CreateNewWindowParams& create_new_window_params,
    const WebContents::CreateParams& web_contents_create_params) {
  PrerenderHost* host =
      GetPrerenderHostRegistry().FindNonReservedHostById(prerender_host_id_);
  if (!host) {
    // The host was already canceled, for example, due to disallowed feature
    // usage in the prerendered page, while this PrerenderNewTabHandle is still
    // alive.
    return nullptr;
  }
  if (host->GetInitialUrl() != create_new_window_params.target_url) {
    // The host is not eligible for the target URL.
    return nullptr;
  }

  // Verify the opener frame is the same with the frame that triggered
  // prerendering.
  if (web_contents_create_params_.opener_render_process_id !=
      web_contents_create_params.opener_render_process_id) {
    return nullptr;
  }
  if (web_contents_create_params_.opener_render_frame_id !=
      web_contents_create_params.opener_render_frame_id) {
    return nullptr;
  }

  // TODO(crbug.com/40234240): Consider supporting activation for non-empty
  // `main_frame_name`.
  CHECK(web_contents_create_params_.main_frame_name.empty());
  if (web_contents_create_params_.main_frame_name !=
      web_contents_create_params.main_frame_name) {
    return nullptr;
  }

  // TODO(crbug.com/40234240): Compare other parameters on CreateNewWindowParams
  // and WebContents::CreateParams. Also, we could have some guard to make sure
  // that parameters newly added to WebContents::CreateParams are accordingly
  // handled here with an approach similar to SameSizeAsDocumentLoader.

  CHECK(web_contents_);
  web_contents_->SetDelegate(nullptr);
  return std::move(web_contents_);
}

PrerenderHostRegistry& PrerenderNewTabHandle::GetPrerenderHostRegistry() {
  CHECK(web_contents_);
  return *web_contents_->GetPrerenderHostRegistry();
}

}  // namespace content
