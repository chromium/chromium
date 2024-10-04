// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_reporter.h"

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/pass_key.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/interest_group/interest_group_pa_report_util.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kReportingBeaconNetworkTag =
    net::DefineNetworkTrafficAnnotation("fenced_frame_reporting_beacon",
                                        R"(
        semantics {
          sender: "Fenced frame reportEvent API"
          description:
            "This request sends out reporting beacon data in an HTTP POST "
            "request. This is initiated by window.fence.reportEvent API."
          trigger:
            "When there are events such as impressions, user interactions and "
            "clicks, fenced frames can invoke window.fence.reportEvent API. It "
            "tells the browser to send a beacon with event data to a URL "
            "registered by the worklet in registerAdBeacon. Please see "
            "https://github.com/WICG/turtledove/blob/main/Fenced_Frames_Ads_Reporting.md#reportevent"
          data:
            "Event data given by fenced frame reportEvent API. Please see "
            "https://github.com/WICG/turtledove/blob/main/Fenced_Frames_Ads_Reporting.md#parameters"
          destination: OTHER
          destination_other: "The reporting destination given by FLEDGE's "
                             "registerAdBeacon API or selectURL's inputs."
          internal {
            contacts {
              email: "chrome-fenced-frames@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-01-04"
        }
        policy {
          cookies_allowed: NO
          setting: "To use reportEvent API, users need to enable selectURL, "
          "FLEDGE and FencedFrames features by enabling the Privacy Sandbox "
          "Ads APIs experiment flag at "
          "chrome://flags/#privacy-sandbox-ads-apis "
          policy_exception_justification: "This beacon is sent by fenced frame "
          "calling window.fence.reportEvent when there are events like user "
          "interactions."
        }
      )");

std::string_view ReportingDestinationAsString(
    const blink::FencedFrame::ReportingDestination& destination) {
  switch (destination) {
    case blink::FencedFrame::ReportingDestination::kBuyer:
      return "Buyer";
    case blink::FencedFrame::ReportingDestination::kSeller:
      return "Seller";
    case blink::FencedFrame::ReportingDestination::kComponentSeller:
      return "ComponentSeller";
    case blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl:
      return "SharedStorageSelectUrl";
    case blink::FencedFrame::ReportingDestination::kDirectSeller:
      return "DirectSeller";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string_view InvokingAPIAsString(
    const PrivacySandboxInvokingAPI invoking_api) {
  switch (invoking_api) {
    case PrivacySandboxInvokingAPI::kProtectedAudience:
      return "Protected Audience";
    case PrivacySandboxInvokingAPI::kSharedStorage:
      return "Shared Storage";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string AutomaticBeaconTypeAsString(
    const blink::mojom::AutomaticBeaconType type) {
  switch (type) {
    case blink::mojom::AutomaticBeaconType::kDeprecatedTopNavigation:
      return blink::kDeprecatedFencedFrameTopNavigationBeaconType;
    case blink::mojom::AutomaticBeaconType::kTopNavigationStart:
      return blink::kFencedFrameTopNavigationStartBeaconType;
    case blink::mojom::AutomaticBeaconType::kTopNavigationCommit:
      return blink::kFencedFrameTopNavigationCommitBeaconType;
    default:
      return "";
  }
}

blink::FencedFrameBeaconReportingResult CreateBeaconReportingResultEnum(
    const FencedFrameReporter::DestinationVariant& event_variant,
    std::optional<int> http_response_code) {
  // Unfortunately absl::visit can't make this more compact, because each
  // combination of results produces a unique output enum.
  if (absl::holds_alternative<DestinationEnumEvent>(event_variant)) {
    if (!http_response_code.has_value()) {
      return blink::FencedFrameBeaconReportingResult::kDestinationEnumInvalid;
    }
    if (*http_response_code != 200) {
      return blink::FencedFrameBeaconReportingResult::kDestinationEnumFailure;
    }
    return blink::FencedFrameBeaconReportingResult::kDestinationEnumSuccess;
  }

  if (absl::holds_alternative<DestinationURLEvent>(event_variant)) {
    if (!http_response_code.has_value()) {
      return blink::FencedFrameBeaconReportingResult::kDestinationUrlInvalid;
    }
    if (*http_response_code != 200) {
      return blink::FencedFrameBeaconReportingResult::kDestinationUrlFailure;
    }
    return blink::FencedFrameBeaconReportingResult::kDestinationUrlSuccess;
  }

  if (absl::holds_alternative<AutomaticBeaconEvent>(event_variant)) {
    if (!http_response_code.has_value()) {
      return blink::FencedFrameBeaconReportingResult::kAutomaticInvalid;
    }
    if (*http_response_code != 200) {
      return blink::FencedFrameBeaconReportingResult::kAutomaticFailure;
    }
    return blink::FencedFrameBeaconReportingResult::kAutomaticSuccess;
  }

  return blink::FencedFrameBeaconReportingResult::kUnknownResult;
}

void RecordBeaconReportingResultHistogram(
    const FencedFrameReporter::DestinationVariant& event_variant,
    net::HttpResponseHeaders* headers) {
  std::optional<int> http_response_code;

  if (headers != nullptr) {
    http_response_code = headers->response_code();
  }

  base::UmaHistogramEnumeration(
      blink::kFencedFrameBeaconReportingHttpResultUMA,
      CreateBeaconReportingResultEnum(event_variant, http_response_code));
}

}  // namespace

FencedFrameReporter::PendingEvent::PendingEvent(
    const DestinationVariant& event,
    const url::Origin& request_initiator,
    const net::ReferrerPolicy request_referrer_policy,
    std::optional<AttributionReportingData> attribution_reporting_data,
    FrameTreeNodeId initiator_frame_tree_node_id)
    : event(event),
      request_initiator(request_initiator),
      request_referrer_policy(request_referrer_policy),
      attribution_reporting_data(std::move(attribution_reporting_data)),
      initiator_frame_tree_node_id(initiator_frame_tree_node_id) {}

FencedFrameReporter::PendingEvent::PendingEvent(const PendingEvent&) = default;

FencedFrameReporter::PendingEvent::PendingEvent(PendingEvent&&) = default;

FencedFrameReporter::PendingEvent& FencedFrameReporter::PendingEvent::operator=(
    const PendingEvent&) = default;

FencedFrameReporter::PendingEvent& FencedFrameReporter::PendingEvent::operator=(
    PendingEvent&&) = default;

FencedFrameReporter::PendingEvent::~PendingEvent() = default;

FencedFrameReporter::ReportingDestinationInfo::ReportingDestinationInfo(
    std::optional<url::Origin> reporting_url_declarer_origin,
    std::optional<ReportingUrlMap> reporting_url_map)
    : reporting_url_declarer_origin(reporting_url_declarer_origin),
      reporting_url_map(std::move(reporting_url_map)) {}

FencedFrameReporter::ReportingDestinationInfo::ReportingDestinationInfo(
    ReportingDestinationInfo&&) = default;

FencedFrameReporter::ReportingDestinationInfo::~ReportingDestinationInfo() =
    default;

FencedFrameReporter::ReportingDestinationInfo&
FencedFrameReporter::ReportingDestinationInfo::operator=(
    ReportingDestinationInfo&&) = default;

scoped_refptr<FencedFrameReporter> FencedFrameReporter::CreateForSharedStorage(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    BrowserContext* browser_context,
    const std::optional<url::Origin>& reporting_url_declarer_origin,
    ReportingUrlMap reporting_url_map,
    const url::Origin& main_frame_origin) {
  // `private_aggregation_manager_` and `winner_origin_`
  // are only needed by FLEDGE.
  scoped_refptr<FencedFrameReporter> reporter =
      base::MakeRefCounted<FencedFrameReporter>(
          base::PassKey<FencedFrameReporter>(),
          PrivacySandboxInvokingAPI::kSharedStorage,
          std::move(url_loader_factory), browser_context, main_frame_origin);
  reporter->reporting_metadata_.emplace(
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      ReportingDestinationInfo(reporting_url_declarer_origin,
                               std::move(reporting_url_map)));
  return reporter;
}

scoped_refptr<FencedFrameReporter> FencedFrameReporter::CreateForFledge(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    BrowserContext* browser_context,
    bool direct_seller_is_seller,
    PrivateAggregationManager* private_aggregation_manager,
    const url::Origin& main_frame_origin,
    const url::Origin& winner_origin,
    const std::optional<url::Origin>& aggregation_coordinator_origin,
    const std::optional<std::vector<url::Origin>>& allowed_reporting_origins) {
  scoped_refptr<FencedFrameReporter> reporter =
      base::MakeRefCounted<FencedFrameReporter>(
          base::PassKey<FencedFrameReporter>(),
          PrivacySandboxInvokingAPI::kProtectedAudience,
          std::move(url_loader_factory), browser_context, main_frame_origin,
          private_aggregation_manager, winner_origin,
          aggregation_coordinator_origin, allowed_reporting_origins);
  reporter->direct_seller_is_seller_ = direct_seller_is_seller;
  reporter->reporting_metadata_.emplace(
      blink::FencedFrame::ReportingDestination::kBuyer,
      ReportingDestinationInfo());
  reporter->reporting_metadata_.emplace(
      blink::FencedFrame::ReportingDestination::kSeller,
      ReportingDestinationInfo());
  reporter->reporting_metadata_.emplace(
      blink::FencedFrame::ReportingDestination::kComponentSeller,
      ReportingDestinationInfo());
  return reporter;
}

FencedFrameReporter::FencedFrameReporter(
    base::PassKey<FencedFrameReporter> pass_key,
    PrivacySandboxInvokingAPI invoking_api,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    BrowserContext* browser_context,
    const url::Origin& main_frame_origin,
    PrivateAggregationManager* private_aggregation_manager,
    const std::optional<url::Origin>& winner_origin,
    const std::optional<url::Origin>& winner_aggregation_coordinator_origin,
    const std::optional<std::vector<url::Origin>>& allowed_reporting_origins)
    : url_loader_factory_(std::move(url_loader_factory)),
      attribution_manager_(
          AttributionManager::FromBrowserContext(browser_context)),
      browser_context_(browser_context),
      main_frame_origin_(main_frame_origin),
      private_aggregation_manager_(private_aggregation_manager),
      winner_origin_(winner_origin),
      winner_aggregation_coordinator_origin_(
          winner_aggregation_coordinator_origin),
      allowed_reporting_origins_(allowed_reporting_origins),
      invoking_api_(invoking_api) {
  DCHECK(url_loader_factory_);
  DCHECK(browser_context_);

  // `winner_origin` should have a value if and only if this a Protected
  // Audience reporter.
  DCHECK_EQ(invoking_api == PrivacySandboxInvokingAPI::kProtectedAudience,
            winner_origin.has_value());
}

FencedFrameReporter::~FencedFrameReporter() {
  for (const auto& [destination, destination_info] : reporting_metadata_) {
    for (const auto& pending_event : destination_info.pending_events) {
      NotifyFencedFrameReportingBeaconFailed(
          pending_event.attribution_reporting_data);
    }
  }

  base::UmaHistogramCustomCounts(blink::kFencedFrameBeaconReportingCountUMA,
                                 beacons_sent_same_origin_, /*min=*/1,
                                 /*exclusive_max=*/20, /*buckets=*/20);
  base::UmaHistogramCustomCounts(
      blink::kFencedFrameBeaconReportingCountCrossOriginUMA,
      beacons_sent_cross_origin_, /*min=*/1, /*exclusive_max=*/20,
      /*buckets=*/20);
}

void FencedFrameReporter::OnUrlMappingReady(
    blink::FencedFrame::ReportingDestination reporting_destination,
    const std::optional<url::Origin>& reporting_url_declarer_origin,
    ReportingUrlMap reporting_url_map,
    std::optional<ReportingMacros> reporting_ad_macros) {
  auto it = reporting_metadata_.find(reporting_destination);
  CHECK(it != reporting_metadata_.end(), base::NotFatalUntil::M130);
  DCHECK(!it->second.reporting_url_map);
  DCHECK(!it->second.reporting_ad_macros);

  it->second.reporting_url_declarer_origin = reporting_url_declarer_origin;
  it->second.reporting_url_map = std::move(reporting_url_map);
  it->second.reporting_ad_macros = std::move(reporting_ad_macros);
  auto pending_events = std::exchange(it->second.pending_events, {});
  for (const auto& pending_event : pending_events) {
    std::string ignored_error_message;
    blink::mojom::ConsoleMessageLevel ignored_console_message_level =
        blink::mojom::ConsoleMessageLevel::kError;
    const std::string devtools_request_id =
        base::UnguessableToken::Create().ToString();
    SendReportInternal(
        it->second, pending_event.event, reporting_destination,
        pending_event.request_initiator, pending_event.request_referrer_policy,
        pending_event.attribution_reporting_data,
        pending_event.initiator_frame_tree_node_id, ignored_error_message,
        ignored_console_message_level, devtools_request_id);
  }
}

bool FencedFrameReporter::SendReport(
    const DestinationVariant& event_variant,
    blink::FencedFrame::ReportingDestination reporting_destination,
    RenderFrameHostImpl* request_initiator_frame,
    std::string& error_message,
    blink::mojom::ConsoleMessageLevel& console_message_level,
    FrameTreeNodeId initiator_frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  DCHECK(request_initiator_frame);

  if (reporting_destination ==
      blink::FencedFrame::ReportingDestination::kDirectSeller) {
    if (direct_seller_is_seller_) {
      reporting_destination = blink::FencedFrame::ReportingDestination::kSeller;
    } else {
      reporting_destination =
          blink::FencedFrame::ReportingDestination::kComponentSeller;
    }
  }
  auto it = reporting_metadata_.find(reporting_destination);
  // Check metadata registration for given destination. If there's no map, or
  // the map is empty, can't send a request. An entry with a null (not empty)
  // map means the map is pending, and is handled below.
  if (it == reporting_metadata_.end() ||
      (absl::holds_alternative<DestinationEnumEvent>(event_variant) &&
       it->second.reporting_url_map && it->second.reporting_url_map->empty())) {
    error_message = base::StrCat(
        {"This frame did not register reporting metadata for destination '",
         ReportingDestinationAsString(reporting_destination), "'."});
    console_message_level = blink::mojom::ConsoleMessageLevel::kWarning;
    return false;
  }

  static base::AtomicSequenceNumber unique_id_counter;

  std::optional<AttributionReportingData> attribution_reporting_data;

  const std::string devtools_request_id =
      base::UnguessableToken::Create().ToString();

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(request_initiator_frame);
  if (web_contents) {
    network::mojom::AttributionSupport attribution_reporting_support =
        static_cast<WebContentsImpl*>(web_contents)->GetAttributionSupport();
    auto suitable_context =
        AttributionSuitableContext::Create(request_initiator_frame);
    if (suitable_context.has_value()) {
      BeaconId beacon_id(unique_id_counter.GetNext());

      AttributionDataHostManager* manager =
          suitable_context->data_host_manager();
      manager->NotifyFencedFrameReportingBeaconStarted(
          beacon_id, std::move(*suitable_context), navigation_id,
          devtools_request_id);

      attribution_reporting_data.emplace(AttributionReportingData{
          .beacon_id = beacon_id,
          .is_automatic_beacon = navigation_id.has_value(),
          .attribution_reporting_support = attribution_reporting_support,
      });
    }
  }

  url::Origin request_initiator =
      request_initiator_frame->GetLastCommittedOrigin();
  net::ReferrerPolicy request_referrer_policy = net::ReferrerPolicy::ORIGIN;

  if (request_initiator_frame->policy_container_host()) {
    request_referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
        request_initiator_frame->policy_container_host()->referrer_policy());
  }

  // Automatic beacons that originate from component ads shouldn't expose the ad
  // component's origin in the referrer for the beacon or the frame's referrer
  // policy. Instead, use the origin and referrer policy of the ad frame root.
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesReportEventHeaderChanges) &&
      request_initiator_frame->frame_tree_node()->GetFencedFrameProperties() &&
      request_initiator_frame->frame_tree_node()
          ->GetFencedFrameProperties()
          ->is_ad_component()) {
    FrameTreeNode* ad_component_root =
        request_initiator_frame->frame_tree_node()
            ->GetClosestAncestorWithFencedFrameProperties();
    FrameTreeNode* ad_root =
        ad_component_root->GetParentOrOuterDocument()
            ->frame_tree_node()
            ->GetClosestAncestorWithFencedFrameProperties();
    CHECK(absl::holds_alternative<AutomaticBeaconEvent>(event_variant));
    request_initiator = ad_root->current_frame_host()->GetLastCommittedOrigin();
    request_referrer_policy =
        Referrer::ReferrerPolicyForUrlRequest(ad_root->current_frame_host()
                                                  ->policy_container_host()
                                                  ->referrer_policy());
  }

  // If the reporting URL map is pending, queue the event.
  NotifyIsBeaconQueued(
      event_variant,
      /*is_queued=*/it->second.reporting_url_map == std::nullopt);

  if (it->second.reporting_url_map == std::nullopt) {
    it->second.pending_events.emplace_back(
        event_variant, request_initiator, request_referrer_policy,
        std::move(attribution_reporting_data), initiator_frame_tree_node_id);
    return true;
  }

  return SendReportInternal(it->second, event_variant, reporting_destination,
                            request_initiator, request_referrer_policy,
                            attribution_reporting_data,
                            initiator_frame_tree_node_id, error_message,
                            console_message_level, devtools_request_id);
}

bool FencedFrameReporter::SendReportInternal(
    const ReportingDestinationInfo& reporting_destination_info,
    const DestinationVariant& event_variant,
    blink::FencedFrame::ReportingDestination reporting_destination,
    const url::Origin& request_initiator,
    const net::ReferrerPolicy request_referrer_policy,
    const std::optional<AttributionReportingData>& attribution_reporting_data,
    FrameTreeNodeId initiator_frame_tree_node_id,
    std::string& error_message,
    blink::mojom::ConsoleMessageLevel& console_message_level,
    const std::string& devtools_request_id) {
  // The URL map should not be pending at this point.
  CHECK(reporting_destination_info.reporting_url_map.has_value());

  // Compute the destination url for the report, and the origin that we will
  // use as the initiator for the report's network request.
  GURL destination_url;
  url::Origin network_request_initiator = request_initiator;
  if (absl::holds_alternative<DestinationEnumEvent>(event_variant) ||
      absl::holds_alternative<AutomaticBeaconEvent>(event_variant)) {
    std::string event_type;

    if (absl::holds_alternative<DestinationEnumEvent>(event_variant)) {
      event_type = absl::get<DestinationEnumEvent>(event_variant).type;
    } else {
      event_type = AutomaticBeaconTypeAsString(
          absl::get<AutomaticBeaconEvent>(event_variant).type);
    }

    // Since the event references a destination enum, resolve the lookup based
    // on the given destination and event type using the reporting metadata.
    const auto url_iter =
        reporting_destination_info.reporting_url_map->find(event_type);
    if (url_iter == reporting_destination_info.reporting_url_map->end()) {
      error_message = base::StrCat(
          {"This frame did not register reporting url for destination '",
           ReportingDestinationAsString(reporting_destination),
           "' and event_type '", event_type, "'."});
      console_message_level = blink::mojom::ConsoleMessageLevel::kWarning;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // Validate the reporting URL.
    destination_url = url_iter->second;
    if (!destination_url.is_valid() || !destination_url.SchemeIsHTTPOrHTTPS()) {
      error_message = base::StrCat(
          {"This frame registered invalid reporting url for destination '",
           ReportingDestinationAsString(reporting_destination),
           "' and event_type '", event_type, "'."});
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // Because the destination URL was chosen by the worklet and is unknown to
    // the reportEvent caller, set `network_request_initiator` to the worklet's
    // origin to prevent CSRF.
    if (base::FeatureList::IsEnabled(
            blink::features::kFencedFramesAutomaticBeaconCredentials)) {
      CHECK(
          reporting_destination_info.reporting_url_declarer_origin.has_value());
      network_request_initiator =
          reporting_destination_info.reporting_url_declarer_origin.value();
    }
  } else {
    // Since the event references a destination URL, use it directly.
    // The URL should have been validated previously, to be a valid HTTPS URL.
    CHECK(absl::holds_alternative<DestinationURLEvent>(event_variant));

    // Check that reportEvent to custom destination URLs with macro
    // substitution is allowed in this context. (i.e., The macro map has a
    // value.)
    if (!reporting_destination_info.reporting_ad_macros.has_value()) {
      error_message =
          "This frame attempted to send a report to a custom destination URL "
          "with macro substitution, which is not supported by the API that "
          "created this frame's fenced frame config.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // If there is no allowlist, or the allowlist is empty, provide a more
    // specific error message.
    if (!allowed_reporting_origins_.has_value() ||
        allowed_reporting_origins_->empty()) {
      error_message =
          "This frame attempted to send a report to a custom destination URL "
          "with macro substitution, but no origins are allowed by its "
          "allowlist.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // If the origin allowlist has previously been violated, this feature is
    // disabled for the lifetime of the FencedFrameReporter. This prevents
    // an interest group from encoding cross-site data about a user in binary
    // with its choices of allowed/disallowed origins.
    if (attempted_custom_url_report_to_disallowed_origin_) {
      error_message =
          "This frame attempted to send a report to a custom destination URL "
          "with macro substitution, but this functionality is disabled because "
          "a request was previously attempted to a disallowed origin.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    const GURL& original_url =
        absl::get<DestinationURLEvent>(event_variant).url;
    if (!original_url.is_valid() || !original_url.SchemeIs(url::kHttpsScheme)) {
      attempted_custom_url_report_to_disallowed_origin_ = true;
      error_message =
          "This frame attempted to send a report to an invalid custom "
          "destination URL. No further reports to custom destination URLs will "
          "be allowed for this fenced frame config.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // Substitute macros in the specified URL using the macros.
    destination_url = GURL(SubstituteMappedStrings(
        original_url.spec(),
        reporting_destination_info.reporting_ad_macros.value()));
    if (!destination_url.is_valid() ||
        !destination_url.SchemeIs(url::kHttpsScheme)) {
      attempted_custom_url_report_to_disallowed_origin_ = true;
      error_message =
          "This frame attempted to send a report to a custom destination URL "
          "that is invalid after macro substitution. No further reports to "
          "custom destination URLs will be allowed for this fenced frame "
          "config.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }

    // Check whether the destination URL has an allowed origin.
    url::Origin destination_origin = url::Origin::Create(destination_url);
    bool is_allowed_origin = false;
    for (auto& origin : allowed_reporting_origins_.value()) {
      if (origin.IsSameOriginWith(destination_origin)) {
        is_allowed_origin = true;
        break;
      }
    }

    // If the destination URL has a disallowed origin, disable this feature for
    // the lifetime of the FencedFrameReporter and return.
    if (!is_allowed_origin) {
      attempted_custom_url_report_to_disallowed_origin_ = true;
      error_message =
          "This frame attempted to send a report to a custom destination URL "
          "with macro substitution to a disallowed origin. No further reports "
          "to custom destination URLs will be allowed for this fenced frame "
          "config.";
      console_message_level = blink::mojom::ConsoleMessageLevel::kError;
      NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
      return false;
    }
  }

  if (!GetContentClient()
           ->browser()
           ->IsPrivacySandboxReportingDestinationAttested(
               browser_context_, url::Origin::Create(destination_url),
               invoking_api_)) {
    error_message = base::StrCat(
        {"The reporting destination '",
         ReportingDestinationAsString(reporting_destination),
         "' is not attested for '", InvokingAPIAsString(invoking_api_), "'"});
    console_message_level = blink::mojom::ConsoleMessageLevel::kError;
    NotifyFencedFrameReportingBeaconFailed(attribution_reporting_data);
    return false;
  }

  // Construct the resource request.
  auto request = std::make_unique<network::ResourceRequest>();

  request->url = destination_url;
  request->mode = network::mojom::RequestMode::kCors;
  request->request_initiator = network_request_initiator;

  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Allow cookies on automatic beacons while third party cookies are enabled
  // to help with adoption/debugging.
  // (https://github.com/WICG/turtledove/issues/866)
  // TODO(crbug.com/40286778): After 3PCD, this will be dead code and should be
  // removed.
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesAutomaticBeaconCredentials) &&
      absl::holds_alternative<AutomaticBeaconEvent>(event_variant) &&
      GetContentClient()
          ->browser()
          ->AreDeprecatedAutomaticBeaconCredentialsAllowed(
              browser_context_, destination_url, main_frame_origin_)) {
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  }
  if (absl::holds_alternative<DestinationURLEvent>(event_variant)) {
    request->method = net::HttpRequestHeaders::kGetMethod;
  } else {
    request->method = net::HttpRequestHeaders::kPostMethod;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesReportEventHeaderChanges)) {
    // For automatic beacons initiating from component ad frames, the
    // request_initiator will have already been set to the root ad frame's
    // origin by this point. For all cases, the request initiator will always be
    // sanitized to just its origin.
    request->referrer_policy = request_referrer_policy;
    request->referrer = request_initiator.GetURL();
  }
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();

  // `attribution_reporting_data` is guaranteed to be set iff attribution
  // reporting is allowed in the initiator frame.
  const bool is_attribution_reporting_allowed =
      attribution_reporting_data.has_value();

  if (attribution_manager_ && is_attribution_reporting_allowed) {
    request->attribution_reporting_eligibility =
        attribution_reporting_data->is_automatic_beacon
            ? network::mojom::AttributionReportingEligibility::kNavigationSource
            : network::mojom::AttributionReportingEligibility::kEventSource;

    request->attribution_reporting_support =
        attribution_reporting_data->attribution_reporting_support;
  }

  request->devtools_request_id = devtools_request_id;
  FrameTreeNode* initiator_frame_tree_node =
      FrameTreeNode::GloballyFindByID(initiator_frame_tree_node_id);
  if (initiator_frame_tree_node) {
    request->trusted_params->devtools_observer =
        NetworkServiceDevToolsObserver::MakeSelfOwned(
            initiator_frame_tree_node);
  }

  std::optional<std::string> event_data;
  if (absl::holds_alternative<DestinationEnumEvent>(event_variant)) {
    event_data.emplace(absl::get<DestinationEnumEvent>(event_variant).data);
  }
  if (absl::holds_alternative<AutomaticBeaconEvent>(event_variant)) {
    event_data.emplace(absl::get<AutomaticBeaconEvent>(event_variant).data);
  }

  devtools_instrumentation::OnFencedFrameReportRequestSent(
      initiator_frame_tree_node_id, devtools_request_id, *request,
      event_data.value_or(""));

  // Create and configure `SimpleURLLoader` instance.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       kReportingBeaconNetworkTag);
  if (event_data.has_value()) {
    simple_url_loader->AttachStringForUpload(
        event_data.value(), /*upload_content_type=*/"text/plain;charset=UTF-8");
  }

  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  AttributionDataHostManager* attribution_data_host_manager =
      attribution_manager_ ? attribution_manager_->GetDataHostManager()
                           : nullptr;

  if (attribution_data_host_manager && is_attribution_reporting_allowed) {
    // Notify Attribution Reporting API for the beacons.
    simple_url_loader_ptr->SetOnRedirectCallback(base::BindRepeating(
        [](base::WeakPtr<AttributionDataHostManager>
               attribution_data_host_manager,
           BeaconId beacon_id, const GURL& url_before_redirect,
           const net::RedirectInfo& redirect_info,
           const network::mojom::URLResponseHead& response_head,
           std::vector<std::string>* removed_headers) {
          if (attribution_data_host_manager) {
            attribution_data_host_manager->NotifyFencedFrameReportingBeaconData(
                beacon_id, url_before_redirect, response_head.headers.get(),
                /*is_final_response=*/false);
          }
        },
        attribution_data_host_manager->AsWeakPtr(),
        attribution_reporting_data->beacon_id));

    // Send out the reporting beacon.
    simple_url_loader_ptr->DownloadHeadersOnly(
        url_loader_factory_.get(),
        base::BindOnce(
            [](DestinationVariant event_variant,
               base::WeakPtr<AttributionDataHostManager>
                   attribution_data_host_manager,
               BeaconId beacon_id,
               std::unique_ptr<network::SimpleURLLoader> loader,
               FrameTreeNodeId initiator_frame_tree_node_id,
               std::string devtools_request_id,
               scoped_refptr<net::HttpResponseHeaders> headers) {
              if (attribution_data_host_manager) {
                attribution_data_host_manager
                    ->NotifyFencedFrameReportingBeaconData(
                        beacon_id, loader->GetFinalURL(), headers.get(),
                        /*is_final_response=*/true);
              }
              // Set up DevTools integration for the response.
              devtools_instrumentation::OnFencedFrameReportResponseReceived(
                  initiator_frame_tree_node_id, devtools_request_id,
                  loader->GetFinalURL(), headers);

              // Record UMA metrics for the destination.
              RecordBeaconReportingResultHistogram(event_variant,
                                                   headers.get());
            },
            event_variant, attribution_data_host_manager->AsWeakPtr(),
            attribution_reporting_data->beacon_id, std::move(simple_url_loader),
            initiator_frame_tree_node_id, devtools_request_id));
  } else {
    // Send out the reporting beacon.
    simple_url_loader_ptr->DownloadHeadersOnly(
        url_loader_factory_.get(),
        base::BindOnce(
            [](DestinationVariant event_variant,
               std::unique_ptr<network::SimpleURLLoader> loader,
               FrameTreeNodeId initiator_frame_tree_node_id,
               std::string devtools_request_id,
               scoped_refptr<net::HttpResponseHeaders> headers) {
              // Set up DevTools integration for the response.
              devtools_instrumentation::OnFencedFrameReportResponseReceived(
                  initiator_frame_tree_node_id, devtools_request_id,
                  loader->GetFinalURL(), headers);

              // Record UMA metrics for the destination.
              RecordBeaconReportingResultHistogram(event_variant,
                                                   headers.get());
            },
            event_variant, std::move(simple_url_loader),
            initiator_frame_tree_node_id, devtools_request_id));
  }

  // The associated histograms will be sent out in the FencedFrameReporter
  // destructor.
  absl::visit(
      [&](const auto& event) {
        using Event = std::decay_t<decltype(event)>;
        if constexpr (std::is_same_v<Event, DestinationEnumEvent> ||
                      std::is_same_v<Event, DestinationURLEvent>) {
          if (event.cross_origin_exposed) {
            beacons_sent_cross_origin_++;
          } else {
            beacons_sent_same_origin_++;
          }
        }
      },
      event_variant);

  return true;
}

void FencedFrameReporter::AddObserverForTesting(ObserverForTesting* observer) {
  observers_.AddObserver(observer);
}

void FencedFrameReporter::RemoveObserverForTesting(
    const ObserverForTesting* observer) {
  observers_.RemoveObserver(observer);
}

void FencedFrameReporter::OnForEventPrivateAggregationRequestsReceived(
    std::map<std::string, PrivateAggregationRequests>
        private_aggregation_event_map) {
  for (auto& [event_type, requests] : private_aggregation_event_map) {
    PrivateAggregationRequests& destination_vector =
        private_aggregation_event_map_[event_type];
    destination_vector.insert(destination_vector.end(),
                              std::move_iterator(requests.begin()),
                              std::move_iterator(requests.end()));
  }

  for (const std::string& pa_event_type : received_pa_events_) {
    SendPrivateAggregationRequestsForEventInternal(pa_event_type);
  }
}

void FencedFrameReporter::SendPrivateAggregationRequestsForEvent(
    const std::string& pa_event_type) {
  if (!private_aggregation_manager_) {
    // `private_aggregation_manager_` is nullptr when private aggregation
    // feature flag is disabled, but a compromised renderer might still send
    // events when it should not be able to. Simply ignores the events.
    return;
  }

  // Always insert `pa_event_type` to `received_pa_events_`, since
  // `private_aggregation_event_map_` might grow with more entries when
  // reportWin() completes.
  received_pa_events_.emplace(pa_event_type);

  SendPrivateAggregationRequestsForEventInternal(pa_event_type);
}

void FencedFrameReporter::SendPrivateAggregationRequestsForEventInternal(
    const std::string& pa_event_type) {
  DCHECK(private_aggregation_manager_);
  DCHECK(winner_origin_.has_value() &&
         winner_origin_.value().scheme() == url::kHttpsScheme);
  DCHECK(main_frame_origin_.scheme() == url::kHttpsScheme);

  auto it = private_aggregation_event_map_.find(pa_event_type);
  if (it == private_aggregation_event_map_.end()) {
    return;
  }

  SplitContributionsIntoBatchesThenSendToHost(
      /*requests=*/std::move(it->second), *private_aggregation_manager_,
      /*reporting_origin=*/winner_origin_.value(),
      /*aggregation_coordinator_origin=*/winner_aggregation_coordinator_origin_,
      main_frame_origin_);

  // Remove the entry of key `pa_event_type` from
  // `private_aggregation_event_map_` to avoid possibly sending the same
  // requests more than once. As a result, receiving the same event type
  // multiple times only triggers sending the event's requests once.
  private_aggregation_event_map_.erase(it);
}

const std::vector<blink::FencedFrame::ReportingDestination>
FencedFrameReporter::ReportingDestinations() {
  std::vector<blink::FencedFrame::ReportingDestination> out;
  for (const auto& reporting_metadata : reporting_metadata_) {
    // Only add the reporting destination if the URL map has at least 1 entry.
    // If the reporting URL map is null, it hasn't been received yet, so add
    // the destination in case the URL map ends up populated with at least 1
    // entry.
    if (!reporting_metadata.second.reporting_url_map ||
        !reporting_metadata.second.reporting_url_map->empty()) {
      out.emplace_back(reporting_metadata.first);
    }
  }
  return out;
}

base::flat_map<blink::FencedFrame::ReportingDestination, url::Origin>
FencedFrameReporter::GetReportingUrlDeclarerOriginsForTesting() {
  base::flat_map<blink::FencedFrame::ReportingDestination, url::Origin> out;
  for (const auto& reporting_metadata : reporting_metadata_) {
    if (reporting_metadata.second.reporting_url_declarer_origin) {
      out.emplace(reporting_metadata.first,
                  *reporting_metadata.second.reporting_url_declarer_origin);
    }
  }
  return out;
}

base::flat_map<blink::FencedFrame::ReportingDestination,
               FencedFrameReporter::ReportingUrlMap>
FencedFrameReporter::GetAdBeaconMapForTesting() {
  base::flat_map<blink::FencedFrame::ReportingDestination, ReportingUrlMap> out;
  for (const auto& reporting_metadata : reporting_metadata_) {
    if (reporting_metadata.second.reporting_url_map) {
      out.emplace(reporting_metadata.first,
                  *reporting_metadata.second.reporting_url_map);
    }
  }
  return out;
}

base::flat_map<blink::FencedFrame::ReportingDestination,
               FencedFrameReporter::ReportingMacros>
FencedFrameReporter::GetAdMacrosForTesting() {
  base::flat_map<blink::FencedFrame::ReportingDestination, ReportingMacros> out;
  for (const auto& reporting_metadata : reporting_metadata_) {
    if (reporting_metadata.second.reporting_ad_macros) {
      out.emplace(reporting_metadata.first,
                  *reporting_metadata.second.reporting_ad_macros);
    }
  }
  return out;
}

std::set<std::string> FencedFrameReporter::GetReceivedPaEventsForTesting()
    const {
  return received_pa_events_;
}

std::map<std::string, FencedFrameReporter::PrivateAggregationRequests>
FencedFrameReporter::GetPrivateAggregationEventMapForTesting() {
  std::map<std::string, FencedFrameReporter::PrivateAggregationRequests> out;
  for (auto& [event_type, requests] : private_aggregation_event_map_) {
    for (auction_worklet::mojom::PrivateAggregationRequestPtr& request :
         requests) {
      out[event_type].emplace_back(request.Clone());
    }
  }
  return out;
}

void FencedFrameReporter::NotifyFencedFrameReportingBeaconFailed(
    const std::optional<AttributionReportingData>& attribution_reporting_data) {
  if (!attribution_reporting_data.has_value()) {
    return;
  }

  AttributionDataHostManager* attribution_data_host_manager =
      attribution_manager_ ? attribution_manager_->GetDataHostManager()
                           : nullptr;
  if (!attribution_data_host_manager) {
    return;
  }

  attribution_data_host_manager->NotifyFencedFrameReportingBeaconData(
      attribution_reporting_data->beacon_id,
      /*reporting_url=*/GURL(), /*headers=*/nullptr,
      /*is_final_response=*/true);
}

void FencedFrameReporter::NotifyIsBeaconQueued(
    const DestinationVariant& event_variant,
    bool is_queued) {
  for (ObserverForTesting& observer : observers_) {
    observer.OnBeaconQueued(event_variant, is_queued);
  }
}

}  // namespace content
