// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "url/gurl.h"
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
  raw_ptr<Map> map_;
  typename Map::iterator it_;
};

}  // namespace

AttributionHost::AttributionHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<AttributionHost>(*web_contents),
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
      !AttributionManager::FromWebContents(web_contents())) {
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

void AttributionHost::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  auto it =
      navigation_impression_origins_.find(navigation_handle->GetNavigationId());
  if (it == navigation_impression_origins_.end())
    return;

  DCHECK(navigation_handle->GetImpression());

  std::string source_header;
  if (!navigation_handle->GetResponseHeaders()->GetNormalizedHeader(
          "Attribution-Reporting-Register-Source", &source_header)) {
    return;
  }

  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  if (!attribution_manager)
    return;

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  const url::Origin& impression_origin = it->second;

  const std::vector<GURL>& redirect_chain =
      navigation_handle->GetRedirectChain();

  if (redirect_chain.size() < 2)
    return;

  // The reporting origin should be the origin of the request responsible for
  // initiating this redirect. At this point, the navigation handle reflects the
  // URL being navigated to, so instead use the second to last URL in the
  // redirect chain.
  url::Origin reporting_origin =
      url::Origin::Create(redirect_chain[redirect_chain.size() - 2]);

  data_host_manager->NotifyNavigationRedirectRegistation(
      navigation_handle->GetImpression()->attribution_src_token, source_header,
      std::move(reporting_origin), impression_origin);
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
      AttributionManager::FromWebContents(web_contents());
  if (!attribution_manager) {
    DCHECK(navigation_impression_origins_.empty());
    if (navigation_handle->GetImpression())
      RecordRegisterImpressionAllowed(false);
    return;
  }

  ScopedMapDeleter<NavigationImpressionOriginMap>
      navigation_impression_origin_it(&navigation_impression_origins_,
                                      navigation_handle->GetNavigationId());

  // Separate from above because we need to clear the navigation related state
  if (!navigation_handle->HasCommitted()) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }

  // Don't observe error page navs, and don't let impressions be registered for
  // error pages.
  if (navigation_handle->IsErrorPage()) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }

  // If we were not able to access the impression origin, ignore the
  // navigation.
  if (!navigation_impression_origin_it) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }
  const url::Origin& impression_origin =
      (*navigation_impression_origin_it.get())->second;

  DCHECK(navigation_handle->GetImpression());
  const blink::Impression& impression = *(navigation_handle->GetImpression());

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  const url::Origin& destination_origin =
      navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin();

  data_host_manager->NotifyNavigationForDataHost(
      impression.attribution_src_token, impression_origin, destination_origin);
}

void AttributionHost::MaybeNotifyFailedSourceNavigation(
    NavigationHandle* navigation_handle) {
  auto* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  if (!attribution_manager)
    return;

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  absl::optional<blink::Impression> impression =
      navigation_handle->GetImpression();
  if (!impression)
    return;

  data_host_manager->NotifyNavigationFailure(impression->attribution_src_token);
}

void AttributionHost::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  // If there is no attribution manager available, ignore any registrations.
  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
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
      AttributionManager::FromWebContents(web_contents());
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

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  if (!data_host_manager->RegisterNavigationDataHost(std::move(data_host),
                                                     attribution_src_token)) {
    mojo::ReportBadMessage(
        "Renderer attempted to register a data host with a duplicate "
        "AttribtionSrcToken.");
    return;
  }
}

// static
void AttributionHost::BindReceiver(
    mojo::PendingAssociatedReceiver<blink::mojom::ConversionHost> receiver,
    RenderFrameHost* rfh) {
  auto* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* conversion_host = AttributionHost::FromWebContents(web_contents);
  if (!conversion_host)
    return;
  conversion_host->receivers_.Bind(rfh, std::move(receiver));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AttributionHost);

}  // namespace content
