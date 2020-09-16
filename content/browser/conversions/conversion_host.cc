// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_page_metrics.h"
#include "content/browser/conversions/conversion_policy.h"
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
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

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

}  // namespace

// static
std::unique_ptr<ConversionHost> ConversionHost::CreateForTesting(
    WebContents* web_contents,
    std::unique_ptr<ConversionManager::Provider> conversion_manager_provider) {
  return base::WrapUnique(
      new ConversionHost(web_contents, std::move(conversion_manager_provider)));
}

ConversionHost::ConversionHost(WebContents* web_contents)
    : ConversionHost(web_contents,
                     std::make_unique<ConversionManagerProviderImpl>()) {}

ConversionHost::ConversionHost(
    WebContents* web_contents,
    std::unique_ptr<ConversionManager::Provider> conversion_manager_provider)
    : WebContentsObserver(web_contents),
      conversion_manager_provider_(std::move(conversion_manager_provider)),
      receiver_(web_contents, this) {
  // TODO(csharrison): When https://crbug.com/1051334 is resolved, add a DCHECK
  // that the kConversionMeasurement feature is enabled.
}

ConversionHost::~ConversionHost() {
  DCHECK_EQ(0u, navigation_impression_origins_.size());
}

void ConversionHost::DidStartNavigation(NavigationHandle* navigation_handle) {
  // Navigations with an impression set should only occur in the main frame.
  if (!navigation_handle->GetImpression() ||
      !navigation_handle->IsInMainFrame() ||
      !conversion_manager_provider_->GetManager(web_contents())) {
    return;
  }

  RenderFrameHostImpl* initiator_frame_host =
      RenderFrameHostImpl::FromID(navigation_handle->GetInitiatorRoutingId());

  // The initiator frame host may be deleted by this point. In that case, ignore
  // this navigation and drop the impression associated with it.
  // TODO(https://crbug.com/1056907): Record metrics on how often impressions
  // are dropped because the initiator is destroyed.
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
}

void ConversionHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager) {
    DCHECK(navigation_impression_origins_.empty());
    return;
  }

  ScopedMapDeleter<NavigationImpressionOriginMap> it(
      &navigation_impression_origins_, navigation_handle->GetNavigationId());

  // If an impression is not associated with a main frame navigation, ignore it.
  // If the navigation did not commit, committed to a Chrome error page, or was
  // same document, ignore it. Impressions should never be attached to
  // same-document navigations but can be the result of a bad renderer.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  conversion_page_metrics_ = std::make_unique<ConversionPageMetrics>();

  // If we were not able to access the impression origin, ignore the navigation.
  if (!it)
    return;
  url::Origin impression_origin = std::move((*it.get())->second);
  DCHECK(navigation_handle->GetImpression());
  const Impression& impression = *(navigation_handle->GetImpression());

  // If the impression's conversion destination does not match the final top
  // frame origin of this new navigation ignore it.
  if (impression.conversion_destination !=
      navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin()) {
    return;
  }

  if (!GetContentClient()->browser()->AllowConversionMeasurement(
          web_contents()->GetBrowserContext())) {
    return;
  }

  // Convert |impression| into a StorableImpression that can be forwarded to
  // storage. If a reporting origin was not provided, default to the conversion
  // destination for reporting.
  const url::Origin& reporting_origin = !impression.reporting_origin
                                            ? impression_origin
                                            : *impression.reporting_origin;

  // Conversion measurement is only allowed in secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(impression_origin) ||
      !network::IsOriginPotentiallyTrustworthy(reporting_origin) ||
      !network::IsOriginPotentiallyTrustworthy(
          impression.conversion_destination)) {
    // TODO (1049654): This should log a console error when it occurs.
    return;
  }

  base::Time impression_time = base::Time::Now();
  const ConversionPolicy& policy = conversion_manager->GetConversionPolicy();
  StorableImpression storable_impression(
      policy.GetSanitizedImpressionData(impression.impression_data),
      impression_origin, impression.conversion_destination, reporting_origin,
      impression_time,
      policy.GetExpiryTimeForImpression(impression.expiry, impression_time),
      /*impression_id=*/base::nullopt);

  conversion_manager->HandleImpression(storable_impression);
}

void ConversionHost::RegisterConversion(
    blink::mojom::ConversionPtr conversion) {
  content::RenderFrameHost* render_frame_host =
      receiver_.GetCurrentTargetFrame();

  // Conversion registration is only allowed in the main frame.
  if (render_frame_host->GetParent()) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used by the main frame.");
    return;
  }

  // If there is no conversion manager available, ignore any conversion
  // registrations.
  ConversionManager* conversion_manager =
      conversion_manager_provider_->GetManager(web_contents());
  if (!conversion_manager)
    return;

  // Only allow conversion registration on secure pages with a secure conversion
  // redirects.
  if (!network::IsOriginPotentiallyTrustworthy(
          render_frame_host->GetLastCommittedOrigin()) ||
      !network::IsOriginPotentiallyTrustworthy(conversion->reporting_origin)) {
    mojo::ReportBadMessage(
        "blink.mojom.ConversionHost can only be used in secure contexts with a "
        "secure conversion registration origin.");
    return;
  }

  if (!GetContentClient()->browser()->AllowConversionMeasurement(
          web_contents()->GetBrowserContext())) {
    return;
  }

  StorableConversion storable_conversion(
      conversion_manager->GetConversionPolicy().GetSanitizedConversionData(
          conversion->conversion_data),
      render_frame_host->GetLastCommittedOrigin(),
      conversion->reporting_origin);

  if (conversion_page_metrics_)
    conversion_page_metrics_->OnConversion(storable_conversion);
  conversion_manager->HandleConversion(storable_conversion);
}

void ConversionHost::SetCurrentTargetFrameForTesting(
    RenderFrameHost* render_frame_host) {
  receiver_.SetCurrentTargetFrameForTesting(render_frame_host);
}

}  // namespace content
