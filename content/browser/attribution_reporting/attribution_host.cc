// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_host_utils.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_metrics.h"
#include "content/browser/attribution_reporting/attribution_page_metrics.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/url_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

AttributionHost* g_receiver_for_testing = nullptr;

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
    if (*this)
      map_->erase(it_);
  }

  typename Map::iterator* get() { return &it_; }

  explicit operator bool() const { return it_ != map_->end(); }

 private:
  raw_ptr<Map> map_;
  typename Map::iterator it_;
};

}  // namespace

AttributionHost::AttributionHost(WebContents* web_contents)
    : AttributionHost(web_contents, AttributionManagerProvider::Default()) {}

AttributionHost::AttributionHost(
    WebContents* web_contents,
    std::unique_ptr<AttributionManagerProvider> attribution_manager_provider)
    : WebContentsObserver(web_contents),
      WebContentsUserData<AttributionHost>(*web_contents),
      attribution_manager_provider_(std::move(attribution_manager_provider)),
      receivers_(web_contents, this) {
  // TODO(csharrison): When https://crbug.com/1051334 is resolved, add a DCHECK
  // that the kConversionMeasurement feature is enabled.
}

AttributionHost::~AttributionHost() {
  DCHECK_EQ(0u, navigation_impression_origins_.size());
}

void AttributionHost::DidStartNavigation(NavigationHandle* navigation_handle) {
  // Impression navigations need to navigate the primary main frame to be valid.
  if (!navigation_handle->GetImpression() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      !attribution_manager_provider_->GetManager(web_contents())) {
    return;
  }

  // There's no initiator frame for App-initiated origins, and so no work is
  // required at navigation start time.
  if (IsAndroidAppOrigin(navigation_handle->GetInitiatorOrigin()))
    return;

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

  if (!initiator_frame_host)
    return;

  // Look up the initiator root's origin which will be used as the impression
  // origin. This works because we won't update the origin for the initiator RFH
  // until we receive confirmation from the renderer that it has committed.
  // Since frame mutation is all serialized on the Blink main thread, we get an
  // implicit ordering: a navigation with an impression attached won't be
  // processed after a navigation commit in the initiator RFH, so reading the
  // origin off is safe at the start of the navigation.
  const url::Origin& initiator_root_frame_origin =
      initiator_frame_host->frame_tree_node()
          ->frame_tree()
          ->root()
          ->current_origin();
  navigation_impression_origins_.emplace(navigation_handle->GetNavigationId(),
                                         initiator_root_frame_origin);

  if (auto* initiator_web_contents =
          WebContents::FromRenderFrameHost(initiator_frame_host)) {
    if (auto* initiator_conversion_host =
            AttributionHost::FromWebContents(initiator_web_contents)) {
      // This doesn't necessarily mean that the browser will store the report,
      // due to the additional logic in DidFinishNavigation(). This records
      // that a page /attempted/ to register an impression for a navigation.
      initiator_conversion_host->NotifyImpressionInitiatedByPage(
          initiator_root_frame_origin, *(navigation_handle->GetImpression()));
    }
  }
}

void AttributionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // Observe only navigation toward a new document in the primary main frame.
  // Impressions should never be attached to same-document navigations but can
  // be the result of a bad renderer.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }

  AttributionManager* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager) {
    DCHECK(navigation_impression_origins_.empty());
    DCHECK(!pending_attribution_);
    if (navigation_handle->GetImpression())
      RecordRegisterImpressionAllowed(false);
    return;
  }

  ScopedMapDeleter<NavigationImpressionOriginMap>
      navigation_impression_origin_it(&navigation_impression_origins_,
                                      navigation_handle->GetNavigationId());

  absl::optional<PendingAttribution> pending_attribution =
      std::move(pending_attribution_);
  pending_attribution_ = absl::nullopt;

  // Separate from above because we need to clear the navigation related state
  if (!navigation_handle->HasCommitted()) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }

  // Don't observe error page navs, and don't let impressions be registered for
  // error pages.
  if (navigation_handle->IsErrorPage()) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    last_navigation_allows_attribution_ = false;
    return;
  }

  // We have a new cross-document navigation.
  last_navigation_allows_attribution_ = true;

  conversion_page_metrics_ = std::make_unique<AttributionPageMetrics>();
  bool is_android_app_origin =
      IsAndroidAppOrigin(navigation_handle->GetInitiatorOrigin()) ||
      (pending_attribution &&
       IsAndroidAppOrigin(pending_attribution->initiator_origin));

  // If we were not able to access the impression origin, ignore the
  // navigation.
  if (!navigation_impression_origin_it && !pending_attribution &&
      !is_android_app_origin) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }
  const url::Origin& impression_origin =
      pending_attribution
          ? pending_attribution->initiator_origin
          : (is_android_app_origin
                 ? *navigation_handle->GetInitiatorOrigin()
                 : (*navigation_impression_origin_it.get())->second);

  DCHECK(navigation_handle->GetImpression() || pending_attribution);
  const blink::Impression& impression =
      pending_attribution ? pending_attribution->impression
                          : *(navigation_handle->GetImpression());

  if (impression.attribution_src_token) {
    auto* data_host_manager = attribution_manager->GetDataHostManager();
    if (!data_host_manager)
      return;

    const url::Origin& destination_origin =
        navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin();

    data_host_manager->NotifyNavigationForDataHost(
        *impression.attribution_src_token, impression_origin,
        destination_origin);
    return;
  }

  // If the impression's conversion destination does not match the final top
  // frame origin of this new navigation ignore it.
  if (net::SchemefulSite(impression.conversion_destination) !=
      net::SchemefulSite(
          navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin())) {
    return;
  }

  attribution_host_utils::VerifyAndStoreImpression(
      AttributionSourceType::kNavigation, impression_origin, impression,
      *attribution_manager, base::Time::Now());
}

void AttributionHost::MaybeNotifyFailedSourceNavigation(
    NavigationHandle* navigation_handle) {
  auto* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager)
    return;

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  absl::optional<blink::Impression> impression =
      navigation_handle->GetImpression();
  if (!impression || !impression->attribution_src_token) {
    return;
  }

  data_host_manager->NotifyNavigationFailure(
      *impression->attribution_src_token);
}

void AttributionHost::RegisterConversion(
    blink::mojom::ConversionPtr conversion) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();

  // AttributionHost calls are delayed in blink if pre-rendering.
  DCHECK_NE(content::RenderFrameHost::LifecycleState::kPrerendering,
            render_frame_host->GetLifecycleState());

  // If there is no conversion manager available, ignore any conversion
  // registrations.
  AttributionManager* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager) {
    RecordRegisterConversionAllowed(false);
    return;
  }

  const url::Origin& conversion_origin =
      render_frame_host->GetLastCommittedOrigin();
  const url::Origin& main_frame_origin =
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin();

  // Only allow conversion registration on secure pages with a secure conversion
  // redirects.
  if (!network::IsOriginPotentiallyTrustworthy(conversion_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts.");
        return;
  }
  if (!network::IsOriginPotentiallyTrustworthy(conversion->reporting_origin)) {
    mojo::ReportBadMessage("blink.mojom.ConversionHost can only be used with "
        "a secure conversion registration origin.");
        return;
  }
  if (!network::IsOriginPotentiallyTrustworthy(main_frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used with a secure top-level "
        "frame.");
    return;
  }

  AttributionTrigger storable_conversion(
      conversion->conversion_data, /*destination_origin=*/main_frame_origin,
      conversion->reporting_origin, conversion->event_source_trigger_data,
      conversion->priority,
      conversion->dedup_key.is_null()
          ? absl::nullopt
          : absl::make_optional(conversion->dedup_key->value),
      /*debug_key=*/absl::nullopt, AttributionAggregatableTrigger());

  if (conversion_page_metrics_)
    conversion_page_metrics_->OnConversion(conversion->reporting_origin);

  attribution_manager->HandleTrigger(std::move(storable_conversion));
}

void AttributionHost::NotifyImpressionInitiatedByPage(
    const url::Origin& impression_origin,
    const blink::Impression& impression) {
  if (!conversion_page_metrics_)
    return;

  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression_origin
                                            : *impression.reporting_origin;
  conversion_page_metrics_->OnImpression(reporting_origin);
}

void AttributionHost::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  // If there is no attribution manager available, ignore any registrations.
  AttributionManager* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager)
    return;

  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();

  const url::Origin& frame_origin = render_frame_host->GetLastCommittedOrigin();
  const url::Origin& top_frame_origin =
      render_frame_host->GetOutermostMainFrame()->GetLastCommittedOrigin();

  if (!network::IsOriginPotentiallyTrustworthy(top_frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used with a secure top-level "
        "frame.");
    return;
  }

  if (render_frame_host != render_frame_host->GetOutermostMainFrame() &&
      !network::IsOriginPotentiallyTrustworthy(frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts.");
    return;
  }

  if (!attribution_manager->GetDataHostManager())
    return;

  attribution_manager->GetDataHostManager()->RegisterDataHost(
      std::move(data_host), top_frame_origin);
}

void AttributionHost::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token) {
  // If there is no attribution manager available, ignore any registrations.
  AttributionManager* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager)
    return;

  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();

  const url::Origin& frame_origin = render_frame_host->GetLastCommittedOrigin();
  const url::Origin& top_frame_origin =
      render_frame_host->GetOutermostMainFrame()->GetLastCommittedOrigin();

  if (!network::IsOriginPotentiallyTrustworthy(top_frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used with a secure top-level "
        "frame.");
    return;
  }

  if (render_frame_host != render_frame_host->GetOutermostMainFrame() &&
      !network::IsOriginPotentiallyTrustworthy(frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts.");
    return;
  }

  if (!attribution_manager->GetDataHostManager())
    return;

  attribution_manager->GetDataHostManager()->RegisterNavigationDataHost(
      std::move(data_host), attribution_src_token);
}

void AttributionHost::ReportAttributionForCurrentNavigation(
    const url::Origin& impression_origin,
    const blink::Impression& impression) {
  AttributionManager* attribution_manager =
      attribution_manager_provider_->GetManager(web_contents());
  if (!attribution_manager)
    return;
  // If a navigation is ongoing, add the attribution to that navigation.
  if (web_contents()->GetController().GetPendingEntry()) {
    pending_attribution_ = {impression_origin, impression};
    return;
  }

  // The navigation has already committed, so add the attribution to the last
  // committed navigation.

  if (!last_navigation_allows_attribution_)
    return;
  // Prevent multiple attributions using the same navigation.
  last_navigation_allows_attribution_ = false;

  // Ensure the committed origin matches the destination for the conversion,
  // but allow subdomains to differ.
  if (net::SchemefulSite(
          web_contents()->GetMainFrame()->GetLastCommittedOrigin()) !=
      net::SchemefulSite(impression.conversion_destination)) {
    return;
  }

  // No navigation in progress and we've already committed the destination for
  // the conversion, so just store the impression.
  attribution_host_utils::VerifyAndStoreImpression(
      AttributionSourceType::kNavigation, impression_origin, impression,
      *attribution_manager, base::Time::Now());
}

// static
void AttributionHost::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::ConversionHost> receiver,
    RenderFrameHost* rfh) {
  if (g_receiver_for_testing) {
    g_receiver_for_testing->receivers_.Bind(rfh, std::move(receiver));
    return;
  }

  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* conversion_host = AttributionHost::FromWebContents(web_contents);
  if (!conversion_host)
    return;
  conversion_host->receivers_.Bind(rfh, std::move(receiver));
}

// static
blink::mojom::ImpressionPtr AttributionHost::MojoImpressionFromImpression(
    const blink::Impression& impression) {
  return blink::mojom::Impression::New(
      impression.conversion_destination, impression.reporting_origin,
      impression.impression_data, impression.expiry, impression.priority,
      impression.attribution_src_token);
}

// static
void AttributionHost::SetReceiverImplForTesting(AttributionHost* impl) {
  g_receiver_for_testing = impl;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AttributionHost);

}  // namespace content
