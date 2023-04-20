// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"
#endif

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

// Abstraction that wraps an iterator to a map. When this goes out of the scope,
// the underlying iterator is erased from the map. This is useful for control
// flows where map cleanup needs to occur regardless of additional early exit
// logic.
template <typename Map>
class ScopedMapDeleter {
 public:
  ScopedMapDeleter(Map* map, const typename Map::key_type& key)
      : map_(map), it_(map_->find(key)) {}
  ~ScopedMapDeleter() {
    if (*this) {
      map_->erase(it_);
    }
  }

  typename Map::iterator* get() { return &it_; }

  explicit operator bool() const { return it_ != map_->end(); }

 private:
  raw_ptr<Map> map_;
  typename Map::iterator it_;
};

}  // namespace

struct AttributionHost::NavigationInfo {
  SuitableOrigin source_origin;
  AttributionInputEvent input_event;
  bool is_within_fenced_frame;
  GlobalRenderFrameHostId initiator_root_frame_id;
};

AttributionHost::AttributionHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<AttributionHost>(*web_contents),
      receivers_(web_contents, this) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kConversionMeasurement));

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb)) {
    input_event_tracker_android_ =
        std::make_unique<AttributionInputEventTrackerAndroid>(web_contents);
  }
#endif
}

AttributionHost::~AttributionHost() {
  DCHECK_EQ(0u, navigation_info_map_.size());
}

AttributionInputEvent AttributionHost::GetMostRecentNavigationInputEvent()
    const {
  AttributionInputEvent input;
#if BUILDFLAG(IS_ANDROID)
  if (input_event_tracker_android_) {
    input.input_event = input_event_tracker_android_->GetMostRecentEvent();
  }
#endif
  return input;
}

void AttributionHost::DidStartNavigation(NavigationHandle* navigation_handle) {
  const auto& impression = navigation_handle->GetImpression();

  // TODO(crbug.com/1428315): Consider checking for navigations taking place in
  // a prerendered main frame.

  // Impression navigations need to navigate the primary main frame to be valid.
  // Impressions should never be attached to same-document navigations but can
  // be the result of a bad renderer.
  if (!impression || !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  RenderFrameHostImpl* initiator_frame_host =
      navigation_handle->GetInitiatorFrameToken().has_value()
          ? RenderFrameHostImpl::FromFrameToken(
                navigation_handle->GetInitiatorProcessID(),
                navigation_handle->GetInitiatorFrameToken().value())
          : nullptr;

  // The initiator frame host may be deleted by this point. In that case, ignore
  // this navigation and drop the impression associated with it.

  UMA_HISTOGRAM_BOOLEAN("Conversions.ImpressionNavigationHasDeadInitiator",
                        initiator_frame_host == nullptr);

  if (!initiator_frame_host) {
    return;
  }

  if (!initiator_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kAttributionReporting)) {
    return;
  }

  RenderFrameHostImpl* initiator_root_frame =
      initiator_frame_host->GetOutermostMainFrame();
  DCHECK(initiator_root_frame);

  // Look up the initiator root's origin which will be used as the impression
  // origin. This works because we won't update the origin for the initiator RFH
  // until we receive confirmation from the renderer that it has committed.
  // Since frame mutation is all serialized on the Blink main thread, we get an
  // implicit ordering: a navigation with an impression attached won't be
  // processed after a navigation commit in the initiator RFH, so reading the
  // origin off is safe at the start of the navigation.
  absl::optional<SuitableOrigin> initiator_root_frame_origin =
      SuitableOrigin::Create(initiator_root_frame->GetLastCommittedOrigin());

  if (!initiator_root_frame_origin) {
    return;
  }

  auto [it, inserted] = navigation_info_map_.try_emplace(
      navigation_handle->GetNavigationId(),
      NavigationInfo{
          .source_origin = std::move(*initiator_root_frame_origin),
          .input_event =
              AttributionHost::FromWebContents(
                  WebContents::FromRenderFrameHost(initiator_frame_host))
                  ->GetMostRecentNavigationInputEvent(),
          .is_within_fenced_frame =
              initiator_frame_host->IsNestedWithinFencedFrame(),
          .initiator_root_frame_id = initiator_root_frame->GetGlobalId()});
  DCHECK(inserted);

  const NavigationInfo& navigation_info = it->second;

  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  attribution_manager->GetDataHostManager()
      ->NotifyNavigationRegistrationStarted(
          impression->attribution_src_token, navigation_info.source_origin,
          impression->nav_type, navigation_info.is_within_fenced_frame,
          navigation_info.initiator_root_frame_id,
          navigation_handle->GetNavigationId());
}

void AttributionHost::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  NotifyNavigationRegistrationData(navigation_handle,
                                   /*is_final_response=*/false);
}

void AttributionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  ScopedMapDeleter<NavigationInfoMap> navigation_source_origin_it(
      &navigation_info_map_, navigation_handle->GetNavigationId());

  NotifyNavigationRegistrationData(navigation_handle,
                                   /*is_final_response=*/true);
}

void AttributionHost::NotifyNavigationRegistrationData(
    NavigationHandle* navigation_handle,
    bool is_final_response) {
  auto it = navigation_info_map_.find(navigation_handle->GetNavigationId());

  // Observe only navigation toward a new document in the primary main frame.
  // Impressions should never be attached to same-document navigations but can
  // be the result of a bad renderer.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    DCHECK(it == navigation_info_map_.end());
  }
  if (it == navigation_info_map_.end()) {
    return;
  }

  const absl::optional<blink::Impression>& impression =
      navigation_handle->GetImpression();
  if (!impression) {
    return;
  }

  // On redirect, the reporting origin should be the origin of the request
  // responsible for initiating the redirect. At this point, the navigation
  // handle reflects the URL being navigated to, so instead use the second to
  // last URL in the redirect chain.
  //
  // On final response, the reporting origin should be the origin of the request
  // responsible for initiating the last (and only if no redirections) request
  // in the chain.
  const size_t offset = is_final_response ? 1 : 2;
  const std::vector<GURL>& redirect_chain =
      navigation_handle->GetRedirectChain();
  if (redirect_chain.size() < offset) {
    return;
  }
  absl::optional<SuitableOrigin> reporting_origin =
      SuitableOrigin::Create(redirect_chain[redirect_chain.size() - offset]);
  if (!reporting_origin) {
    return;
  }

  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  attribution_manager->GetDataHostManager()->NotifyNavigationRegistrationData(
      impression->attribution_src_token,
      navigation_handle->GetResponseHeaders(), std::move(*reporting_origin),
      it->second.source_origin, it->second.input_event, impression->nav_type,
      it->second.is_within_fenced_frame, it->second.initiator_root_frame_id,
      navigation_handle->GetNavigationId(), is_final_response);
}

absl::optional<SuitableOrigin>
AttributionHost::TopFrameOriginForSecureContext() {
  RenderFrameHostImpl* render_frame_host =
      static_cast<RenderFrameHostImpl*>(receivers_.GetCurrentTargetFrame());

  const url::Origin& top_frame_origin =
      render_frame_host->GetOutermostMainFrame()->GetLastCommittedOrigin();

  // We need a potentially trustworthy origin here because we need to be able to
  // store it as either the source or destination origin. Using
  // `is_web_secure_context` would allow opaque origins to pass through, but
  // they cannot be handled by the storage layer.

  absl::optional<SuitableOrigin> suitable_top_frame_origin =
      SuitableOrigin::Create(top_frame_origin);

  // TODO(crbug.com/1378749): Invoke mojo::ReportBadMessage here when we can be
  // sure honest renderers won't hit this path.
  if (!suitable_top_frame_origin) {
    return absl::nullopt;
  }

  // TODO(crbug.com/1378492): Invoke mojo::ReportBadMessage here when we can be
  // sure honest renderers won't hit this path.
  if (render_frame_host != render_frame_host->GetOutermostMainFrame() &&
      !render_frame_host->policy_container_host()
           ->policies()
           .is_web_secure_context) {
    return absl::nullopt;
  }

  return suitable_top_frame_origin;
}

void AttributionHost::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    attribution_reporting::mojom::RegistrationType registration_type) {
  absl::optional<SuitableOrigin> top_frame_origin =
      TopFrameOriginForSecureContext();
  if (!top_frame_origin) {
    return;
  }

  RenderFrameHostImpl* render_frame_host =
      static_cast<RenderFrameHostImpl*>(receivers_.GetCurrentTargetFrame());
  DCHECK(render_frame_host);

  RenderFrameHostImpl* root_frame_host =
      render_frame_host->GetOutermostMainFrame();
  DCHECK(root_frame_host);

  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  attribution_manager->GetDataHostManager()->RegisterDataHost(
      std::move(data_host), std::move(*top_frame_origin),
      render_frame_host->IsNestedWithinFencedFrame(), registration_type,
      root_frame_host->GetGlobalId(), render_frame_host->navigation_id());
}

void AttributionHost::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  if (!TopFrameOriginForSecureContext()) {
    return;
  }

  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  if (!attribution_manager->GetDataHostManager()->RegisterNavigationDataHost(
          std::move(data_host), attribution_src_token,
          GetMostRecentNavigationInputEvent())) {
    mojo::ReportBadMessage(
        "Renderer attempted to register a data host with a duplicate "
        "AttribtionSrcToken.");
    return;
  }
}

// static
void AttributionHost::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::AttributionHost> receiver,
    RenderFrameHost* rfh) {
  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  auto* attribution_host = AttributionHost::FromWebContents(web_contents);
  if (!attribution_host) {
    return;
  }
  attribution_host->receivers_.Bind(rfh, std::move(receiver));
}

void AttributionHost::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    absl::optional<int64_t> navigation_id,
    RenderFrameHostImpl* initiator_frame_host) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionFencedFrameReportingBeacon)) {
    return;
  }

  if (!initiator_frame_host) {
    return;
  }

  if (!initiator_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kAttributionReporting)) {
    return;
  }

  RenderFrameHostImpl* initiator_root_frame =
      initiator_frame_host->GetOutermostMainFrame();
  DCHECK(initiator_root_frame);

  absl::optional<SuitableOrigin> initiator_root_frame_origin =
      SuitableOrigin::Create(initiator_root_frame->GetLastCommittedOrigin());

  if (!initiator_root_frame_origin) {
    return;
  }

  AttributionInputEvent input_event;
  if (navigation_id.has_value()) {
    input_event = AttributionHost::FromWebContents(
                      WebContents::FromRenderFrameHost(initiator_frame_host))
                      ->GetMostRecentNavigationInputEvent();
  }

  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  attribution_manager->GetDataHostManager()
      ->NotifyFencedFrameReportingBeaconStarted(
          beacon_id, navigation_id, std::move(*initiator_root_frame_origin),
          initiator_frame_host->IsNestedWithinFencedFrame(), input_event,
          initiator_root_frame->GetGlobalId());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AttributionHost);

}  // namespace content
