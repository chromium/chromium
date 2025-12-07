// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/anchor_element_interaction_host_impl.h"

#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-data-view.h"

namespace content {

namespace {

bool IsOutermostMainFrame(const RenderFrameHost& render_frame_host) {
  return !render_frame_host.GetParentOrOuterDocument();
}

void MaybePrewarmHttpDiskCache(const GURL& url,
                               RenderFrameHost& render_frame_host) {
  static const bool enabled =
      base::FeatureList::IsEnabled(blink::features::kHttpDiskCachePrewarming) &&
      blink::features::kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover
          .Get();

  if (!enabled) {
    return;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!IsOutermostMainFrame(render_frame_host)) {
    return;
  }

  // Disallow PrewarmHttpDiskCache when there are other windows that
  // might script with this frame to mitigate security and privacy
  // concerns.
  if (render_frame_host.GetSiteInstance()->GetRelatedActiveContentsCount() >
      1u) {
    return;
  }

  GetContentClient()->browser()->MaybePrewarmHttpDiskCache(
      *render_frame_host.GetBrowserContext(),
      render_frame_host.GetLastCommittedOrigin(), url);
}

void MaybeWarmUpServiceWorker(const GURL& url,
                              RenderFrameHost& render_frame_host) {
  if (!IsOutermostMainFrame(render_frame_host)) {
    return;
  }

  // Disallow service worker warm-up when there are other windows that
  // might script with this frame to mitigate security and privacy
  // concerns.
  if (render_frame_host.GetSiteInstance()->GetRelatedActiveContentsCount() >
      1u) {
    return;
  }

  content::StoragePartition* storage_partition =
      render_frame_host.GetStoragePartition();

  if (!storage_partition) {
    return;
  }

  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();

  if (!service_worker_context) {
    return;
  }

  if (!content::OriginCanAccessServiceWorkers(url)) {
    return;
  }

  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));

  if (!service_worker_context->MaybeHasRegistrationForStorageKey(key)) {
    return;
  }

  service_worker_context->WarmUpServiceWorker(url, key, base::DoNothing());
}

void MaybeWarmUpServiceWorkerOnPointerDown(const GURL& url,
                                           RenderFrameHost& render_frame_host) {
  static const bool enabled =
      base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      blink::features::kSpeculativeServiceWorkerWarmUpOnPointerdown.Get();
  if (enabled) {
    MaybeWarmUpServiceWorker(url, render_frame_host);
  }
}

void MaybeWarmUpServiceWorkerOnPointerHover(
    const GURL& url,
    RenderFrameHost& render_frame_host) {
  static const bool enabled =
      base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      blink::features::kSpeculativeServiceWorkerWarmUpOnPointerover.Get();
  if (enabled) {
    MaybeWarmUpServiceWorker(url, render_frame_host);
  }
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
  MaybeWarmUpServiceWorkerOnPointerDown(url, render_frame_host());
}

void AnchorElementInteractionHostImpl::OnPointerHoverEager(
    const GURL& url,
    blink::mojom::AnchorElementPointerDataPtr mouse_data) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerHover(
      url, std::move(mouse_data), blink::mojom::SpeculationEagerness::kEager);
}

void AnchorElementInteractionHostImpl::OnPointerHoverModerate(
    const GURL& url,
    blink::mojom::AnchorElementPointerDataPtr mouse_data) {
  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnPointerHover(
      url, std::move(mouse_data),
      blink::mojom::SpeculationEagerness::kModerate);
  MaybePrewarmHttpDiskCache(url, render_frame_host());
  MaybeWarmUpServiceWorkerOnPointerHover(url, render_frame_host());
}

void AnchorElementInteractionHostImpl::OnModerateViewportHeuristicTriggered(
    const GURL& url) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPreloadingModerateViewportHeuristics)) {
    ReportBadMessageAndDeleteThis(
        "OnModerateViewportHeuristic should not be called by the renderer "
        "without blink::features::kPreloadingModerateViewportHeuristics being "
        "enabled");
    return;
  }

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  preloading_decider->OnModerateViewportHeuristicTriggered(url);
}

void AnchorElementInteractionHostImpl::OnEagerViewportHeuristicTriggered(
    const std::vector<GURL>& target_urls) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPreloadingEagerViewportHeuristics)) {
    ReportBadMessageAndDeleteThis(
        "OnEagerViewportHeuristic should not be called by the renderer without "
        "blink::features::kPreloadingEagerViewportHeuristics being enabled");
    return;
  }

  auto* preloading_decider =
      PreloadingDecider::GetOrCreateForCurrentDocument(&render_frame_host());
  for (const GURL& url : target_urls) {
    preloading_decider->OnEagerViewportHeuristicTriggered(url);
  }
}

}  // namespace content
