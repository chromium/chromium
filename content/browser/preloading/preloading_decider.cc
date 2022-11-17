// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {
DOCUMENT_USER_DATA_KEY_IMPL(PreloadingDecider);

PreloadingDecider::PreloadingDecider(content::RenderFrameHost* rfh)
    : DocumentUserData<PreloadingDecider>(rfh), prerenderer_(*rfh) {
  preconnect_delegate_ =
      GetContentClient()->browser()->CreateAnchorElementPreconnectDelegate(
          render_frame_host());
}

PreloadingDecider::~PreloadingDecider() = default;

void PreloadingDecider::OnPointerDown(const GURL& url) {
  if (preconnect_delegate_)
    preconnect_delegate_->MaybePreconnect(url);
}

void PreloadingDecider::SetObserverForTesting(
    std::unique_ptr<PreloadingDeciderObserverForTesting> observer) {
  observer_for_testing_ = std::move(observer);
}

void PreloadingDecider::ResetObserverForTesting() {
  observer_for_testing_.reset();
}

void PreloadingDecider::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (observer_for_testing_) {
    observer_for_testing_->UpdateSpeculationCandidates(candidates);
  }
  prerenderer_.ProcessCandidatesForPrerender(candidates);
}

}  // namespace content
