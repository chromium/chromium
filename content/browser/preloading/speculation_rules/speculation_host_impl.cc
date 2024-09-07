// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"

#include <functional>

#include "base/feature_list.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

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

    // Only "prerender" action supports `target_browsing_context_name_hint`.
    // Invalid Speculation Rules are ignored and invalid candidates are not
    // produced in Blink.
    if (candidate->action != blink::mojom::SpeculationAction::kPrerender &&
        candidate->target_browsing_context_name_hint !=
            blink::mojom::SpeculationTargetHint::kNoHint) {
      mojo::ReportBadMessage("SH_TARGET_HINT_ON_PREFETCH");
      return false;
    }

    // Only "prefetch" action supports the requirement
    // "anonymous-client-ip-when-cross-origin". Invalid Speculation Rules are
    // ignored and invalid candidates are not produced in Blink.
    if (candidate->action != blink::mojom::SpeculationAction::kPrefetch &&
        candidate->requires_anonymous_client_ip_when_cross_origin) {
      mojo::ReportBadMessage(
          "SH_INVALID_REQUIRES_ANONYMOUS_CLIENT_IP_WHEN_CROSS_ORIGIN");
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
    : DocumentService(frame_host, std::move(receiver)) {}

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

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->UpdateSpeculationCandidates(candidates);
}

void SpeculationHostImpl::OnLCPPredicted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnLCPPredicted();
}

void SpeculationHostImpl::InitiatePreview(const GURL& url) {
  if (!base::FeatureList::IsEnabled(blink::features::kLinkPreview)) {
    mojo::ReportBadMessage("SH_PREVIEW");
    return;
  }

  // Link Preview is not allowed in a frame with untrusted network disabled.
  if (render_frame_host().IsUntrustedNetworkDisabled()) {
    return;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  CHECK(web_contents);
  WebContentsDelegate* delegate = web_contents->GetDelegate();
  CHECK(delegate);
  delegate->InitiatePreview(*web_contents, url);
}

}  // namespace content
