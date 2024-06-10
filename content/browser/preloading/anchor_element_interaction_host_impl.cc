// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/anchor_element_interaction_host_impl.h"

#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

bool IsOutermostMainFrame(const RenderFrameHost& render_frame_host) {
  return !render_frame_host.GetParentOrOuterDocument();
}

void MaybePrewarmHttpDiskCache(const GURL& url,
                               RenderFrameHost& render_frame_host) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kHttpDiskCachePrewarming) ||
      !blink::features::kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover
           .Get()) {
    return;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!IsOutermostMainFrame(render_frame_host)) {
    return;
  }

  GetContentClient()->browser()->MaybePrewarmHttpDiskCache(
      *render_frame_host.GetBrowserContext(),
      render_frame_host.GetLastCommittedOrigin(), url);
}

}  // namespace

AnchorElementInteractionHostImpl::AnchorElementInteractionHostImpl(
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost> receiver)
    : DocumentService(frame_host, std::move(receiver)) {}

// static
void AnchorElementInteractionHostImpl::Create(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
        receiver) {
  CHECK(frame_host);

  new AnchorElementInteractionHostImpl(*frame_host, std::move(receiver));
}

void AnchorElementInteractionHostImpl::OnPointerDown(const GURL& url) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerDown(url);
  MaybePrewarmHttpDiskCache(url, render_frame_host());
}

void AnchorElementInteractionHostImpl::OnPointerHover(
    const GURL& url,
    blink::mojom::AnchorElementPointerDataPtr mouse_data) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerHover(url, std::move(mouse_data));
  MaybePrewarmHttpDiskCache(url, render_frame_host());
}

}  // namespace content
