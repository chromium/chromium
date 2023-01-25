// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_reporter.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/types/pass_key.h"
#include "components/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "url/gurl.h"
#include "url/origin.h"

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

base::StringPiece ReportingDestinationAsString(
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
  }
}

}  // namespace

FencedFrameReporter::PendingEvent::PendingEvent(
    const std::string& type,
    const std::string& data,
    const url::Origin& request_initiator)
    : type(type), data(data), request_initiator(request_initiator) {}

FencedFrameReporter::ReportingDestinationInfo::ReportingDestinationInfo(
    absl::optional<ReportingUrlMap> reporting_url_map)
    : reporting_url_map(std::move(reporting_url_map)) {}

FencedFrameReporter::ReportingDestinationInfo::ReportingDestinationInfo(
    ReportingDestinationInfo&&) = default;

FencedFrameReporter::ReportingDestinationInfo::~ReportingDestinationInfo() =
    default;

FencedFrameReporter::ReportingDestinationInfo&
FencedFrameReporter::ReportingDestinationInfo::operator=(
    ReportingDestinationInfo&&) = default;

FencedFrameReporter::FencedFrameReporter(
    base::PassKey<FencedFrameReporter> pass_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(url_loader_factory_);
}

FencedFrameReporter::~FencedFrameReporter() = default;

scoped_refptr<FencedFrameReporter> FencedFrameReporter::CreateForSharedStorage(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ReportingUrlMap reporting_url_map) {
  scoped_refptr<FencedFrameReporter> reporter =
      base::MakeRefCounted<FencedFrameReporter>(
          base::PassKey<FencedFrameReporter>(), std::move(url_loader_factory));
  reporter->reporting_metadata_.emplace(
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl,
      ReportingDestinationInfo(std::move(reporting_url_map)));
  return reporter;
}

scoped_refptr<FencedFrameReporter> FencedFrameReporter::CreateForFledge(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  scoped_refptr<FencedFrameReporter> reporter =
      base::MakeRefCounted<FencedFrameReporter>(
          base::PassKey<FencedFrameReporter>(), std::move(url_loader_factory));
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

void FencedFrameReporter::OnUrlMappingReady(
    blink::FencedFrame::ReportingDestination reporting_destination,
    ReportingUrlMap reporting_url_map) {
  auto it = reporting_metadata_.find(reporting_destination);
  DCHECK(it != reporting_metadata_.end());
  DCHECK(!it->second.reporting_url_map);

  it->second.reporting_url_map = std::move(reporting_url_map);
  auto pending_events = std::move(it->second.pending_events);
  for (const auto& pending_event : pending_events) {
    std::string ignored_error_message;
    SendReportInternal(it->second, pending_event.type, pending_event.data,
                       reporting_destination, pending_event.request_initiator,
                       ignored_error_message);
  }
}

bool FencedFrameReporter::SendReport(
    const std::string& event_type,
    const std::string& event_data,
    blink::FencedFrame::ReportingDestination reporting_destination,
    const url::Origin& request_initiator,
    std::string& error_message) {
  auto it = reporting_metadata_.find(reporting_destination);
  // Check metadata registration for given destination. If there's no map, or
  // the map is empty, can't send a request. An entry with a null (not empty)
  // map means the map is pending, and is handled below.
  if (it == reporting_metadata_.end() ||
      (it->second.reporting_url_map && it->second.reporting_url_map->empty())) {
    error_message = base::StrCat(
        {"This frame did not register reporting metadata for destination '",
         ReportingDestinationAsString(reporting_destination), "'."});
    return false;
  }

  // If the reporting URL map is pending, queue the event.
  if (it->second.reporting_url_map == absl::nullopt) {
    it->second.pending_events.emplace_back(event_type, event_data,
                                           request_initiator);
    return true;
  }

  return SendReportInternal(it->second, event_type, event_data,
                            reporting_destination, request_initiator,
                            error_message);
}

bool FencedFrameReporter::SendReportInternal(
    const ReportingDestinationInfo& reporting_destination_info,
    const std::string& event_type,
    const std::string& event_data,
    blink::FencedFrame::ReportingDestination reporting_destination,
    const url::Origin& request_initiator,
    std::string& error_message) {
  // The URL map should not be pending at this point.
  DCHECK(reporting_destination_info.reporting_url_map);

  // Check reporting url registration for given destination and event type.
  const auto url_iter =
      reporting_destination_info.reporting_url_map->find(event_type);
  if (url_iter == reporting_destination_info.reporting_url_map->end()) {
    error_message = base::StrCat(
        {"This frame did not register reporting url for destination '",
         ReportingDestinationAsString(reporting_destination),
         "' and event_type '", event_type, "'."});
    return false;
  }

  // Validate the reporting url.
  GURL url = url_iter->second;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    error_message = base::StrCat(
        {"This frame registered invalid reporting url for destination '",
         ReportingDestinationAsString(reporting_destination),
         "' and event_type '", event_type, "'."});
    return false;
  }

  // Construct the resource request.
  auto request = std::make_unique<network::ResourceRequest>();

  request->url = url;
  request->mode = network::mojom::RequestMode::kCors;
  request->request_initiator = request_initiator;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->method = net::HttpRequestHeaders::kPostMethod;
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateTransient();
  // TODO(xiaochenzh): The eligible header for automatic beacon should be
  // `navigation-source`, update the code below when it is enabled.
  request->headers.SetHeader("Attribution-Reporting-Eligible", "event-source");
  if (base::FeatureList::IsEnabled(
          blink::features::kAttributionReportingCrossAppWeb)) {
    request->headers.SetHeader("Attribution-Reporting-Support",
                               attribution_reporting::GetSupportHeader(
                                   AttributionManager::GetOsSupport()));
  }

  // Create and configure `SimpleURLLoader` instance.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       kReportingBeaconNetworkTag);
  simple_url_loader->AttachStringForUpload(
      event_data, /*upload_content_type=*/"text/plain;charset=UTF-8");

  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  // Send out the reporting beacon.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::DoNothingWithBoundArgs(std::move(simple_url_loader)));
  return true;
}

}  // namespace content
