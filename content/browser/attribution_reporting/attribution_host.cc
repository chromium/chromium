// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
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

struct AttributionHost::NavigationInfo {
  SuitableOrigin source_origin;
  AttributionInputEvent input_event;
};

AttributionHost::AttributionHost(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<AttributionHost>(*web_contents),
      receivers_(web_contents, this) {
  // TODO(csharrison): When https://crbug.com/1051334 is resolved, add a DCHECK
  // that the kConversionMeasurement feature is enabled.

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          blink::features::kAttributionReportingCrossAppWeb)) {
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
  if (input_event_tracker_android_)
    input.input_event = input_event_tracker_android_->GetMostRecentEvent();
#endif
  return input;
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
  absl::optional<SuitableOrigin> initiator_root_frame_origin =
      SuitableOrigin::Create(initiator_frame_host->frame_tree_node()
                                 ->frame_tree()
                                 .root()
                                 ->current_origin());

  if (!initiator_root_frame_origin)
    return;

  navigation_info_map_.emplace(
      navigation_handle->GetNavigationId(),
      NavigationInfo{.source_origin = std::move(*initiator_root_frame_origin),
                     .input_event = AttributionHost::FromWebContents(
                                        WebContents::FromRenderFrameHost(
                                            initiator_frame_host))
                                        ->GetMostRecentNavigationInputEvent()});
}

void AttributionHost::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  auto it = navigation_info_map_.find(navigation_handle->GetNavigationId());
  if (it == navigation_info_map_.end())
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

  const std::vector<GURL>& redirect_chain =
      navigation_handle->GetRedirectChain();

  if (redirect_chain.size() < 2)
    return;

  // The reporting origin should be the origin of the request responsible for
  // initiating this redirect. At this point, the navigation handle reflects the
  // URL being navigated to, so instead use the second to last URL in the
  // redirect chain.
  absl::optional<SuitableOrigin> reporting_origin =
      SuitableOrigin::Create(redirect_chain[redirect_chain.size() - 2]);

  if (!reporting_origin)
    return;

  auto impression = navigation_handle->GetImpression();
  data_host_manager->NotifyNavigationRedirectRegistration(
      navigation_handle->GetImpression()->attribution_src_token,
      std::move(source_header), std::move(*reporting_origin),
      it->second.source_origin, it->second.input_event, impression->nav_type);
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
    DCHECK(navigation_info_map_.empty());
    if (navigation_handle->GetImpression())
      RecordRegisterImpressionAllowed(false);
    return;
  }

  ScopedMapDeleter<NavigationInfoMap> navigation_source_origin_it(
      &navigation_info_map_, navigation_handle->GetNavigationId());

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
  if (!navigation_source_origin_it) {
    MaybeNotifyFailedSourceNavigation(navigation_handle);
    return;
  }
  const SuitableOrigin& source_origin =
      (*navigation_source_origin_it.get())->second.source_origin;

  DCHECK(navigation_handle->GetImpression());
  const blink::Impression& impression = *(navigation_handle->GetImpression());

  auto* data_host_manager = attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  data_host_manager->NotifyNavigationForDataHost(
      impression.attribution_src_token, source_origin, impression.nav_type);
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

  auto dump_without_crashing = [render_frame_host, &top_frame_origin]() {
    SCOPED_CRASH_KEY_STRING1024("", "top_frame_url",
                                render_frame_host->GetOutermostMainFrame()
                                    ->GetLastCommittedURL()
                                    .spec());
    SCOPED_CRASH_KEY_STRING256("", "top_frame_origin",
                               top_frame_origin.Serialize());
    base::debug::DumpWithoutCrashing();
  };

  absl::optional<SuitableOrigin> suitable_top_frame_origin =
      SuitableOrigin::Create(top_frame_origin);

  // TODO(crbug.com/1378749): Invoke mojo::ReportBadMessage here when we can be
  // sure honest renderers won't hit this path.
  if (!suitable_top_frame_origin) {
    dump_without_crashing();
    return absl::nullopt;
  }

  // TODO(crbug.com/1378492): Invoke mojo::ReportBadMessage here when we can be
  // sure honest renderers won't hit this path.
  if (render_frame_host != render_frame_host->GetOutermostMainFrame() &&
      !render_frame_host->policy_container_host()
           ->policies()
           .is_web_secure_context) {
    dump_without_crashing();
    return absl::nullopt;
  }

  return suitable_top_frame_origin;
}

void AttributionHost::RegisterDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    blink::mojom::AttributionRegistrationType registration_type) {
  // If there is no attribution manager available, ignore any registrations.
  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  if (!attribution_manager)
    return;

  AttributionDataHostManager* data_host_manager =
      attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  absl::optional<SuitableOrigin> top_frame_origin =
      TopFrameOriginForSecureContext();
  if (!top_frame_origin)
    return;

  data_host_manager->RegisterDataHost(
      std::move(data_host), std::move(*top_frame_origin),
      receivers_.GetCurrentTargetFrame()->IsNestedWithinFencedFrame(),
      registration_type);
}

void AttributionHost::RegisterNavigationDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
    const blink::AttributionSrcToken& attribution_src_token,
    blink::mojom::AttributionNavigationType nav_type) {
  // If there is no attribution manager available, ignore any registrations.
  AttributionManager* attribution_manager =
      AttributionManager::FromWebContents(web_contents());
  if (!attribution_manager)
    return;

  AttributionDataHostManager* data_host_manager =
      attribution_manager->GetDataHostManager();
  if (!data_host_manager)
    return;

  if (!TopFrameOriginForSecureContext())
    return;

  if (!data_host_manager->RegisterNavigationDataHost(
          std::move(data_host), attribution_src_token,
          GetMostRecentNavigationInputEvent(), nav_type)) {
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
