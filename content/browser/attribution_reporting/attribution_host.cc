// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
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

// Auxiliary data that lives alongside a NavigationHandle, tracking
// whether the navigation is "insecurely tainted" i.e. has seen an
// insecure hop in its path. This should only be present for navigations which
// have had at least one secure response with Attribution headers.
//
// For now, it is only used for metrics collection, but it may be extended in
// the future to block secure registration attempts after an insecure redirect
// was seen. See https://github.com/WICG/attribution-reporting-api/issues/767.
class InsecureTaintTracker
    : public NavigationHandleUserData<InsecureTaintTracker> {
 public:
  explicit InsecureTaintTracker(NavigationHandle&) {}
  ~InsecureTaintTracker() override {
    if (had_any_secure_attempts_) {
      base::UmaHistogramExactLinear("Conversions.IncrementalTaintingFailures",
                                    num_secure_registrations_after_tainting_,
                                    net::URLRequest::kMaxRedirects + 1);
    }
  }

  InsecureTaintTracker(const InsecureTaintTracker&) = delete;
  InsecureTaintTracker& operator=(const InsecureTaintTracker&) = delete;

  void TaintInsecure() { navigation_is_tainted_ = true; }

  void NotifySecureRegistrationAttempt() {
    had_any_secure_attempts_ = true;
    num_secure_registrations_after_tainting_ += navigation_is_tainted_;
  }

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

 private:
  int num_secure_registrations_after_tainting_ = 0;
  bool navigation_is_tainted_ = false;
  bool had_any_secure_attempts_ = false;
};
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(InsecureTaintTracker);

}  // namespace

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
  DCHECK(ongoing_registration_eligible_navigations_.empty());
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
                navigation_handle->GetInitiatorProcessId(),
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

  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  auto* navigation_request = static_cast<NavigationRequest*>(navigation_handle);

  attribution_manager->GetDataHostManager()
      ->NotifyNavigationRegistrationStarted(
          impression->attribution_src_token,
          GetMostRecentNavigationInputEvent(),
          /*source_origin=*/*std::move(initiator_root_frame_origin),
          initiator_frame_host->IsNestedWithinFencedFrame(),
          /*render_frame_id=*/initiator_root_frame->GetGlobalId(),
          navigation_handle->GetNavigationId(),
          // The devtools_navigation_token is going to be used as the
          // navigation's request devtools inspector ID if there is an enabled
          // agent host.
          navigation_request->devtools_navigation_token().ToString());
  auto [_, inserted] = ongoing_registration_eligible_navigations_.emplace(
      navigation_handle->GetNavigationId());
  CHECK(inserted);
}

void AttributionHost::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  NotifyNavigationRegistrationData(navigation_handle,
                                   /*is_final_response=*/false);
}

void AttributionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  const auto& impression = navigation_handle->GetImpression();
  if (!impression.has_value()) {
    return;
  }

  NotifyNavigationRegistrationData(navigation_handle,
                                   /*is_final_response=*/true);

  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  CHECK(attribution_manager);
  attribution_manager->GetDataHostManager()
      ->NotifyNavigationRegistrationCompleted(
          impression->attribution_src_token);

  ongoing_registration_eligible_navigations_.erase(
      navigation_handle->GetNavigationId());
}

void AttributionHost::NotifyNavigationRegistrationData(
    NavigationHandle* navigation_handle,
    bool is_final_response) {
  if (!ongoing_registration_eligible_navigations_.contains(
          navigation_handle->GetNavigationId())) {
    return;
  }

  const absl::optional<blink::Impression>& impression =
      navigation_handle->GetImpression();
  // If there is an ongoing_registration_eligible_navigation, the navigation
  // must have an associated impression, be in the primary main frame and not in
  // the same document.
  DCHECK(impression.has_value());
  DCHECK(navigation_handle->IsInPrimaryMainFrame());
  DCHECK(!navigation_handle->IsSameDocument());

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
  GURL reporting_url = redirect_chain[redirect_chain.size() - offset];
  // Pass the suitability as a proxy for the potentially trustworthy check, as
  // redirects should only happen for HTTP-based navigations.
  auto* tracker =
      InsecureTaintTracker::GetOrCreateForNavigationHandle(*navigation_handle);
  if (!SuitableOrigin::IsSuitable(url::Origin::Create(reporting_url))) {
    tracker->TaintInsecure();
    return;
  }

  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  DCHECK(attribution_manager);

  bool had_header =
      attribution_manager->GetDataHostManager()
          ->NotifyNavigationRegistrationData(
              impression->attribution_src_token,
              navigation_handle->GetResponseHeaders(), std::move(reporting_url),
              impression->runtime_features);

  if (had_header) {
    tracker->NotifySecureRegistrationAttempt();
  }
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
    attribution_reporting::mojom::RegistrationEligibility
        registration_eligibility) {
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
      render_frame_host->IsNestedWithinFencedFrame(), registration_eligibility,
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
          std::move(data_host), attribution_src_token)) {
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

bool AttributionHost::NotifyFencedFrameReportingBeaconStarted(
    BeaconId beacon_id,
    absl::optional<int64_t> navigation_id,
    RenderFrameHostImpl* initiator_frame_host,
    std::string devtools_request_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionFencedFrameReportingBeacon)) {
    return false;
  }

  if (!initiator_frame_host) {
    return false;
  }

  if (!initiator_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kAttributionReporting)) {
    return false;
  }

  RenderFrameHostImpl* initiator_root_frame =
      initiator_frame_host->GetOutermostMainFrame();
  DCHECK(initiator_root_frame);

  absl::optional<SuitableOrigin> initiator_root_frame_origin =
      SuitableOrigin::Create(initiator_root_frame->GetLastCommittedOrigin());

  if (!initiator_root_frame_origin) {
    return false;
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
          initiator_root_frame->GetGlobalId(), std::move(devtools_request_id));
  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AttributionHost);

}  // namespace content
