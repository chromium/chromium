// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_page_metrics.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

ConversionHost* g_receiver_for_testing = nullptr;

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
  Map* map_;
  typename Map::iterator it_;
};

void RecordRegisterConversionAllowed(bool allowed) {
  base::UmaHistogramBoolean("Conversions.RegisterConversionAllowed", allowed);
}

void RecordRegisterImpressionAllowed(bool allowed) {
  base::UmaHistogramBoolean("Conversions.RegisterImpressionAllowed", allowed);
}

bool IsAndroidAppOrigin(const absl::optional<url::Origin>& origin) {
#if defined(OS_ANDROID)
  return origin && origin->scheme() == kAndroidAppScheme;
#else
  return false;
#endif
}

}  // namespace

ConversionHost::ConversionHost(WebContents* web_contents)
    : ConversionHost(web_contents,
                     std::make_unique<ConversionManagerProviderImpl>()) {}

ConversionHost::ConversionHost(
    WebContents* web_contents,
    std::unique_ptr<ConversionManager::Provider> conversion_manager_provider)
    : WebContentsObserver(web_contents),
      conversion_manager_provider_(std::move(conversion_manager_provider)),
      receivers_(web_contents, this) {
  // TODO(csharrison): When https://crbug.com/1051334 is resolved, add a DCHECK
  // that the kConversionMeasurement feature is enabled.
}

ConversionHost::~ConversionHost() {
  DCHECK_EQ(0u, navigation_impression_origins_.size());
}

void ConversionHost::DidStartNavigation(NavigationHandle* navigation_handle) {
  // Impression navigations need to navigate the main frame to be valid.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main
  // frame to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->GetImpression() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      !conversion_manager_provider_->GetManager(web_contents())) {
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
            ConversionHost::FromWebContents(initiator_web_contents)) {
      // This doesn't necessarily mean that the browser will store the report,
      // due to the additional logic in DidFinishNavigation(). This records
      // that a page /attempted/ to register an impression for a navigation.
      initiator_conversion_host->NotifyImpressionNavigationInitiatedByPage();
    }
  }
}

void ConversionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // Observe only navigation toward a new document in the main frame.
  // Impressions should never be attached to same-document navigations but can
  // be the result of a bad renderer.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager) {
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
  if (!navigation_handle->HasCommitted())
    return;

  // Don't observe error page navs, and don't let impressions be registered for
  // error pages.
  if (navigation_handle->IsErrorPage()) {
    last_navigation_allows_attribution_ = false;
    return;
  }

  // We have a new cross-document navigation.
  last_navigation_allows_attribution_ = true;

  conversion_page_metrics_ = std::make_unique<ConversionPageMetrics>();
  bool is_android_app_origin =
      IsAndroidAppOrigin(navigation_handle->GetInitiatorOrigin()) ||
      (pending_attribution &&
       IsAndroidAppOrigin(pending_attribution->initiator_origin));

  // If we were not able to access the impression origin, ignore the
  // navigation.
  if (!navigation_impression_origin_it && !pending_attribution &&
      !is_android_app_origin) {
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

  // If the impression's conversion destination does not match the final top
  // frame origin of this new navigation ignore it.
  if (net::SchemefulSite(impression.conversion_destination) !=
      net::SchemefulSite(
          navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin())) {
    return;
  }

  VerifyAndStoreImpression(StorableImpression::SourceType::kNavigation,
                           impression_origin, impression, *conversion_manager);
}

void ConversionHost::VerifyAndStoreImpression(
    StorableImpression::SourceType source_type,
    const url::Origin& impression_origin,
    const blink::Impression& impression,
    ConversionManager& conversion_manager) {
  // Convert |impression| into a StorableImpression that can be forwarded to
  // storage. If a reporting origin was not provided, default to the conversion
  // destination for reporting.
  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression_origin
                                            : *impression.reporting_origin;

  const bool allowed =
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          web_contents()->GetBrowserContext(),
          ContentBrowserClient::ConversionMeasurementOperation::kImpression,
          &impression_origin, /*conversion_origin=*/nullptr, &reporting_origin);
  RecordRegisterImpressionAllowed(allowed);
  if (!allowed)
    return;

  const bool impression_origin_trustworthy =
      network::IsOriginPotentiallyTrustworthy(impression_origin) ||
      IsAndroidAppOrigin(impression_origin);
  // Conversion measurement is only allowed in secure contexts.
  if (!impression_origin_trustworthy ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    return;
  }

  base::Time impression_time = base::Time::Now();

  const ConversionPolicy& policy = conversion_manager.GetConversionPolicy();
  StorableImpression storable_impression(
      policy.GetSanitizedImpressionData(impression.impression_data),
      impression_origin, impression.conversion_destination, reporting_origin,
      impression_time,
      policy.GetExpiryTimeForImpression(impression.expiry, impression_time,
                                        source_type),
      source_type, impression.priority,
      /*impression_id=*/absl::nullopt);

  conversion_manager.HandleImpression(std::move(storable_impression));
}

void ConversionHost::RegisterConversion(
    blink::mojom::ConversionPtr conversion) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();

  // If there is no conversion manager available, ignore any conversion
  // registrations.
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager) {
    RecordRegisterConversionAllowed(false);
    return;
  }

  const url::Origin& conversion_origin =
      render_frame_host->GetLastCommittedOrigin();
  const url::Origin& main_frame_origin =
      render_frame_host->GetMainFrame()->GetLastCommittedOrigin();

  // Only allow conversion registration on secure pages with a secure conversion
  // redirects.
  if (!network::IsOriginPotentiallyTrustworthy(conversion_origin) ||
      !network::IsOriginPotentiallyTrustworthy(conversion->reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(main_frame_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts with a "
        "secure conversion registration origin.");
    return;
  }

  const bool allowed =
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          web_contents()->GetBrowserContext(),
          ContentBrowserClient::ConversionMeasurementOperation::kConversion,
          /*impression_origin=*/nullptr, &main_frame_origin,
          &conversion->reporting_origin);
  RecordRegisterConversionAllowed(allowed);
  if (!allowed)
    return;

  net::SchemefulSite conversion_destination(main_frame_origin);

  StorableConversion storable_conversion(
      conversion_manager->GetConversionPolicy().GetSanitizedConversionData(
          conversion->conversion_data),
      conversion_destination, conversion->reporting_origin,
      conversion_manager->GetConversionPolicy()
          .GetSanitizedEventSourceTriggerData(
              conversion->event_source_trigger_data),
      conversion->priority,
      conversion->dedup_key.is_null()
          ? absl::nullopt
          : absl::make_optional(conversion->dedup_key->value));

  if (conversion_page_metrics_)
    conversion_page_metrics_->OnConversion();
  conversion_manager->HandleConversion(std::move(storable_conversion));
}

void ConversionHost::NotifyImpressionNavigationInitiatedByPage() {
  if (conversion_page_metrics_)
    conversion_page_metrics_->OnImpression();
}

void ConversionHost::RegisterImpression(const blink::Impression& impression) {
  // If there is no conversion manager available, ignore any impression
  // registrations.
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager)
    return;
  const url::Origin& impression_origin = receivers_.GetCurrentTargetFrame()
                                             ->GetMainFrame()
                                             ->GetLastCommittedOrigin();
  VerifyAndStoreImpression(StorableImpression::SourceType::kEvent,
                           impression_origin, impression, *conversion_manager);
}

void ConversionHost::ReportAttributionForCurrentNavigation(
    const url::Origin& impression_origin,
    const blink::Impression& impression) {
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager)
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
  VerifyAndStoreImpression(StorableImpression::SourceType::kNavigation,
                           impression_origin, impression, *conversion_manager);
}

// static
void ConversionHost::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::ConversionHost> receiver,
    RenderFrameHost* rfh) {
  if (g_receiver_for_testing) {
    g_receiver_for_testing->receivers_.Bind(rfh, std::move(receiver));
    return;
  }

  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* conversion_host = ConversionHost::FromWebContents(web_contents);
  if (!conversion_host)
    return;
  conversion_host->receivers_.Bind(rfh, std::move(receiver));
}

// static
absl::optional<blink::Impression> ConversionHost::ParseImpressionFromApp(
    const std::string& source_event_id,
    const std::string& destination,
    const std::string& report_to,
    int64_t expiry) {
  // Java API should have rejected these already.
  DCHECK(!source_event_id.empty() && !destination.empty());

  blink::Impression impression;
  if (!base::StringToUint64(source_event_id, &impression.impression_data))
    return absl::nullopt;

  impression.conversion_destination = url::Origin::Create(GURL(destination));
  if (!network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    return absl::nullopt;
  }

  if (!report_to.empty()) {
    impression.reporting_origin = url::Origin::Create(GURL(report_to));
    if (!network::IsOriginPotentiallyTrustworthy(*impression.reporting_origin))
      return absl::nullopt;
  }

  if (expiry != 0)
    impression.expiry = base::TimeDelta::FromMilliseconds(expiry);

  return impression;
}

// static
blink::mojom::ImpressionPtr ConversionHost::MojoImpressionFromImpression(
    const blink::Impression& impression) {
  return blink::mojom::Impression::New(
      impression.conversion_destination, impression.reporting_origin,
      impression.impression_data, impression.expiry, impression.priority);
}

// static
void ConversionHost::SetReceiverImplForTesting(ConversionHost* impl) {
  g_receiver_for_testing = impl;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ConversionHost)

}  // namespace content
