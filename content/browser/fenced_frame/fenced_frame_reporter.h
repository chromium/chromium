// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/privacy_sandbox_invoking_api.h"
#include "content/public/browser/render_frame_host.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AttributionManager;
class BrowserContext;
class PrivateAggregationManager;
class RenderFrameHostImpl;

// An event to be sent to a preregistered url.
// `type` is the key for the `ReportingUrlMap`, and `data` is sent with the
// request as a POST.
struct DestinationEnumEvent {
  std::string type;
  std::string data;
  bool cross_origin_exposed;

  // The equal to operator is defined in order to enable comparison of
  // DestinationVariant.
  bool operator==(const DestinationEnumEvent& other) const = default;
};

// An event to be sent to a custom url.
// `url` is the custom destination url, and the request is sent as a GET.
// Macros are substituted using the `ReportingMacros`.
struct DestinationURLEvent {
  GURL url;
  bool cross_origin_exposed;

  // The equal to operator is defined in order to enable comparison of
  // DestinationVariant.
  bool operator==(const DestinationURLEvent& other) const = default;
};

// An event to be sent to a preregistered url as the result of an automatic
// beacon. `type` is the key for the `ReportingUrlMap`, and `data` is sent with
// the request as a POST.
struct AutomaticBeaconEvent {
  blink::mojom::AutomaticBeaconType type;
  std::string data;

  // The equal to operator is defined in order to enable comparison of
  // DestinationVariant.
  bool operator==(const AutomaticBeaconEvent& other) const = default;
};

// Class that receives report events from fenced frames, and uses a
// per-destination-type maps of events to URLs to send reports. The maps may be
// received after the report event calls, in which case the reports will be
// queued until the corresponding map types have been received.
class CONTENT_EXPORT FencedFrameReporter
    : public base::RefCounted<FencedFrameReporter> {
 public:
  using ReportingUrlMap = base::flat_map<std::string, GURL>;

  using ReportingMacros = std::vector<std::pair<std::string, std::string>>;

  using PrivateAggregationRequests =
      std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>;

  using DestinationVariant = absl::
      variant<DestinationEnumEvent, DestinationURLEvent, AutomaticBeaconEvent>;

  // TODO(crbug.com/40285398): Once the CL that stops repeating checks for
  // fenced frame reporting beacons is landed, this observer will be extended to
  // observe whether the beacon is eventually sent or not.
  class ObserverForTesting : public base::CheckedObserver {
   public:
    virtual void OnBeaconQueued(const DestinationVariant& event_variant,
                                bool is_queued) = 0;
  };

  void AddObserverForTesting(ObserverForTesting* observer);
  void RemoveObserverForTesting(const ObserverForTesting* observer);

  // Creates a FencedFrameReporter that only maps kSharedStorageSelectUrl
  // destinations, using the passed in map.
  //
  // `url_loader_factory` is used to send all reports, and must not be null.
  //
  // `browser_context` is used to help notify Attribution Reporting API
  // for the beacons, and to check attestations before sending out the beacons.
  //
  // `reporting_url_declarer_origin` is used to set the request initiator on
  // outgoing beacon network requests. The initiator is the entity that chose
  // the destination URLs in order to prevent CSRF.
  //
  // `main_frame_origin` is the main frame of the page where Shared Storage
  // was called.
  static scoped_refptr<FencedFrameReporter> CreateForSharedStorage(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      BrowserContext* browser_context,
      const std::optional<url::Origin>& reporting_url_declarer_origin,
      ReportingUrlMap reporting_url_map,
      const url::Origin& main_frame_origin = url::Origin());

  // Creates a FencedFrameReporter that maps FLEDGE ReportingDestination types
  // (kBuyer, kSeller, kComponentSeller), but that initially considers all three
  // map types pending, and just collects reporting strings of those types until
  // the corresponding mappings are passed in via OnUrlMappingReady().
  //
  // `url_loader_factory` is used to send all reports, and must not be null.
  //
  // `browser_context` is used to help notify Attribution Reporting API
  // for the beacons, and to check attestations before sending out the beacons.
  //
  // `private_aggregation_manager` is used to send private aggregation requests
  // for fenced frame events. See comment above declaration of
  // `private_aggregation_manager_` for more details.
  //
  // `main_frame_origin` is the main frame of the page where the auction is
  // running. Can be an opaque origin in test iff the test does not have for
  // event private aggregation requests.
  //
  // `winner_origin` is the winning buyer's origin. Can be an opaque origin in
  // test iff the test does not have for event private aggregation requests.
  //
  // `winner_aggregation_coordinator_origin` is the origin of the aggregation
  // coordinator for the winning buyer. Set to std::nullopt if the default
  // coordinator should be used.
  //
  // `allowed_reporting_origins` is the winning ad's allowedReportingOrigins. If
  //  any macro report is attempted to an unlisted origin, all further reports
  //  after it will be cancelled.
  static scoped_refptr<FencedFrameReporter> CreateForFledge(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      BrowserContext* browser_context,
      bool direct_seller_is_seller,
      PrivateAggregationManager* private_aggregation_manager,
      const url::Origin& main_frame_origin,
      const url::Origin& winner_origin,
      const std::optional<url::Origin>& winner_aggregation_coordinator_origin,
      const std::optional<std::vector<url::Origin>>& allowed_reporting_origins =
          std::nullopt);

  // Don't use this constructor directly, but use factory methods instead.
  // See factory methods for details.
  FencedFrameReporter(
      base::PassKey<FencedFrameReporter> pass_key,
      PrivacySandboxInvokingAPI invoking_api,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      BrowserContext* browser_context,
      const url::Origin& main_frame_origin,
      PrivateAggregationManager* private_aggregation_manager = nullptr,
      const std::optional<url::Origin>& winner_origin = std::nullopt,
      const std::optional<url::Origin>& winner_aggregation_coordinator_origin =
          std::nullopt,
      const std::optional<std::vector<url::Origin>>& allowed_reporting_origins =
          std::nullopt);

  // Called when a mapping for reports of type `reporting_destination` is ready.
  // The reporter must currently be considering maps of type
  // `reporting_destination` pending - that is:
  //
  // 1) It must have been created by CreateForFledge()
  // 2) `reporting_destination` must be one of kBuyer, kSeller, kDirectSeller or
  // kComponentSeller.
  // 3) OnUrlMappingReady() must not yet have been invoked with
  // `reporting_destination` yet.
  //
  // When invoked, any pending reports of type `reporting_destination` will be
  // sent if there's a matching entry in `reporting_url_map`. Any future reports
  // of that type will be immediately sent using the provided map. Errors will
  // not be displayed anywhere, as it's unclear where to send them to - the
  // originally associated frame may have already been closed.
  //
  // If it is learned that there are no events types for a particular
  // destination, should be called with an empty ReportingUrlMap for that
  // destination, so it can discard reports for that destination, and provide
  // errors messages for subsequent SendReporter() using that destination.
  //
  // Reports that use the urls declared in `reporting_url_map` will be sent
  // with the request initiator set to `reporting_url_declarer_origin`
  //
  // `reporting_ad_macros` is std::nullopt unless when `reporting_destination`
  // is kBuyer. If it is learned that there are no ad macros for kBuyer, should
  // be called with an empty ReportingMacros, so it can discard macro reports,
  // and provide errors messages for subsequent SendReporter().
  //
  // TODO(crbug.com/40253851): Consider investing in outputting error to
  // correct frame, if it still exists. `frame_tree_node_id` somewhat does this,
  // though it doesn't change across navigations, so could end up displaying an
  // error for a page a frame was previously displaying. There may be other
  // options.
  void OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination reporting_destination,
      const std::optional<url::Origin>& reporting_url_declarer_origin,
      ReportingUrlMap reporting_url_map,
      std::optional<ReportingMacros> reporting_ad_macros = std::nullopt);

  // Sends a report for the specified event, using the ReportingUrlMap
  // associated with `reporting_destination`. If the map for
  // `reporting_destination` is pending, queues the report until the mapping
  // information is received.
  //
  // The event is specified with `event_variant`, which is either:
  // * a `DestinationEnumEvent`, which contains a `type` and `data`
  //   * Sends a POST to the url specified by `type` in the ReportingUrlMap,
  //     with `data` attached.
  //   * If there's no matching `type`, no beacon is sent.
  // * a `DestinationURLEvent`, which contains a `url`
  //   * Sends a GET to `url`.
  //   * Substitutes macros from the ReportingMacros.
  // * an `AutomaticBeaconEvent`, which contains a `type` and `data`
  //   * Like the enum variant, the data is sent as a POST to the url specified
  //     by `type`. However, given that these events are triggered automatically
  //     by the browser, they are semantically different from enum events.
  //
  // Returns false and populated `error_message` and `console_message_level` if
  // no network request was attempted, unless the reporting URL map for
  // `reporting_destination` is pending. In that case, errors are currently
  // never reported, even if the reporting URL map results in no request being
  // sent.
  //
  // `initiator_frame_tree_node_id` is used for DevTools support only.
  //
  // Note: `navigation_id` will only be non-null in the case of an automatic
  // beacon sent as a result of a top-level navigation from a fenced frame. It
  // will be set to the ID of the navigation request initiated from the fenced
  // frame and targeting the new top-level frame. In all other cases (including
  // the fence.reportEvent() case), the navigation id will be null.
  // Note: `ad_root_origin` will only be set for automatic beacons originating
  // from ad components.
  bool SendReport(
      const DestinationVariant& event_variant,
      blink::FencedFrame::ReportingDestination reporting_destination,
      RenderFrameHostImpl* request_initiator_frame,
      std::string& error_message,
      blink::mojom::ConsoleMessageLevel& console_message_level,
      FrameTreeNodeId initiator_frame_tree_node_id = FrameTreeNodeId(),
      std::optional<int64_t> navigation_id = std::nullopt);

  // Called when a mapping for private aggregation requests of non-reserved
  // event types is received. Currently it is only called inside
  // `InterestGroupAuctionReporter::SendPendingReportsIfNavigated()`, which is
  // called after any of the following:
  // * the winning ad has been navigated to.
  // * reportWin() completes.
  // * reportResult() completes.
  // The first two cases can have non-empty `private_aggregation_event_map`.
  // When invoked, any pending non-reserved event type will trigger sending
  // corresponding private aggregation request in
  // `private_aggregation_event_map` if it has a matching key. Any future
  // reports of that type will be immediately sent using the provided map.
  void OnForEventPrivateAggregationRequestsReceived(
      std::map<std::string, PrivateAggregationRequests>
          private_aggregation_event_map);

  // Uses `pa_event_type` to send a private aggregation request. The
  // non-reserved PA event type is added to `received_pa_events_` because more
  // private aggregation requests associated with this event may be received and
  // need to be sent after this is called.
  void SendPrivateAggregationRequestsForEvent(const std::string& pa_event_type);

  // Returns a list of reporting destinations that have at least 1 URL
  // registered with them.
  const std::vector<blink::FencedFrame::ReportingDestination>
  ReportingDestinations();

  // Returns a copy of the internal reporting metadata's
  // `reporting_url_declarer_origin` for each reporting destination, so it can
  // be validated in tests. Only includes reporting destinations for which maps
  // have been received - i.e., if OnUrlMappingReady() has not yet been invoked
  // for a reporting destination, it is not included in the returned map.
  base::flat_map<blink::FencedFrame::ReportingDestination, url::Origin>
  GetReportingUrlDeclarerOriginsForTesting();

  // Returns a copy of the internal reporting metadata's `reporting_url_map`, so
  // it can be validated in tests. Only includes ad beacon maps for which maps
  // have been received - i.e., if OnUrlMappingReady() has not yet been invoked
  // for a reporting destination, it is not included in the returned map.
  base::flat_map<blink::FencedFrame::ReportingDestination, ReportingUrlMap>
  GetAdBeaconMapForTesting();

  // Returns a copy of the internal reporting metadata's
  // `reporting_ad_macros`, so it can be validated in tests. Only includes ad
  // macros for which maps have been received - i.e., if OnUrlMappingReady()
  // has not yet been invoked for a reporting destination, it is not
  // included in the returned map.
  base::flat_map<blink::FencedFrame::ReportingDestination, ReportingMacros>
  GetAdMacrosForTesting();

  // Returns `received_pa_events_`, so that it can be validated in tests. Should
  // only be called from tests.
  std::set<std::string> GetReceivedPaEventsForTesting() const;

  // Returns a copy of `private_aggregation_event_map_`, so that it can be
  // validated in tests. Should only be called from tests.
  std::map<std::string, PrivateAggregationRequests>
  GetPrivateAggregationEventMapForTesting();

 private:
  friend class base::RefCounted<FencedFrameReporter>;
  friend class FencedFrameURLMappingTestPeer;

  struct AttributionReportingData {
    BeaconId beacon_id;
    bool is_automatic_beacon;
    network::mojom::AttributionSupport attribution_reporting_support;
  };

  struct PendingEvent {
    PendingEvent(
        const DestinationVariant& event,
        const url::Origin& request_initiator,
        const net::ReferrerPolicy request_referrer_policy,
        std::optional<AttributionReportingData> attribution_reporting_data,
        FrameTreeNodeId initiator_frame_tree_node_id);

    PendingEvent(const PendingEvent&);
    PendingEvent(PendingEvent&&);

    PendingEvent& operator=(const PendingEvent&);
    PendingEvent& operator=(PendingEvent&&);

    ~PendingEvent();

    DestinationVariant event;
    url::Origin request_initiator;
    net::ReferrerPolicy request_referrer_policy;
    // The data necessary for attribution reporting. Will be `std::nullopt` if
    // attribution reporting is disallowed in the initiator frame.
    std::optional<AttributionReportingData> attribution_reporting_data;
    FrameTreeNodeId initiator_frame_tree_node_id;
  };

  // The per-blink::FencedFrame::ReportingDestination reporting information.
  struct ReportingDestinationInfo {
    explicit ReportingDestinationInfo(
        std::optional<url::Origin> reporting_url_declarer_origin = std::nullopt,
        std::optional<ReportingUrlMap> reporting_url_map = std::nullopt);
    ReportingDestinationInfo(ReportingDestinationInfo&&);
    ~ReportingDestinationInfo();

    ReportingDestinationInfo& operator=(ReportingDestinationInfo&&);

    // `reporting_url_declarer_origin`, `reporting_url_map`, and
    // `reporting_ad_macros` are set asynchronously, all at once.
    // If they are null, any reports that are attempted to be sent of the
    // corresponding type will be added to `pending_events` and only sent once
    // these are populated.
    std::optional<url::Origin> reporting_url_declarer_origin;
    std::optional<ReportingUrlMap> reporting_url_map;
    std::optional<ReportingMacros> reporting_ad_macros;

    // Pending report strings received while `reporting_url_map` was
    // std::nullopt. Once the map is received, this is cleared, and reports are
    // sent. The events are used for post-impression reporting. They are
    // from either `reportEvent()` calls or automatic beacons.
    std::vector<PendingEvent> pending_events;
  };

  ~FencedFrameReporter();

  // Helper to send a report, used by both SendReport() and OnUrlMappingReady().
  bool SendReportInternal(
      const ReportingDestinationInfo& reporting_destination_info,
      const DestinationVariant& event,
      blink::FencedFrame::ReportingDestination reporting_destination,
      const url::Origin& request_initiator,
      const net::ReferrerPolicy request_referrer_policy,
      const std::optional<AttributionReportingData>& attribution_reporting_data,
      FrameTreeNodeId initiator_frame_tree_node_id,
      std::string& error_message,
      blink::mojom::ConsoleMessageLevel& console_message_level,
      const std::string& devtools_request_id);

  // Helper to send private aggregation requests in
  // `private_aggregation_event_map_` with key `pa_event_type`.
  void SendPrivateAggregationRequestsForEventInternal(
      const std::string& pa_event_type);

  // Used by FencedFrameURLMappingTestPeer.
  const base::flat_map<blink::FencedFrame::ReportingDestination,
                       ReportingDestinationInfo>&
  reporting_metadata() const {
    return reporting_metadata_;
  }

  // Helper to notify `AttributionDataHostManager` if the report failed to be
  // sent.
  void NotifyFencedFrameReportingBeaconFailed(
      const std::optional<AttributionReportingData>&
          attribution_reporting_data);

  // Notify the installed observers whether the beacon is queued to be sent or
  // not.
  void NotifyIsBeaconQueued(const DestinationVariant& event_variant,
                            bool is_queued);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Bound to the lifetime of the browser context. Could be null in Incognito
  // mode or in test.
  const raw_ptr<AttributionManager, DanglingUntriaged> attribution_manager_;

  const raw_ptr<BrowserContext> browser_context_;

  base::flat_map<blink::FencedFrame::ReportingDestination,
                 ReportingDestinationInfo>
      reporting_metadata_;

  // True if the "directSeller" alias maps to the Seller destination. False if
  // it maps to the "ComponentSeller" destination.
  bool direct_seller_is_seller_ = false;

  // The origin of the page's main frame. Used for:
  // * Private aggregation (Protected Audience only)
  // * 3rd party cookie permission check for credentialed automatic beacons
  const url::Origin main_frame_origin_;

  // Bound to the lifetime of the browser context. Can be nullptr if:
  // * It's for non-FLEDGE reporter.
  // * In tests that does not trigger private aggregation reports.
  // * When feature `kPrivateAggregationApi` is not enabled.
  const raw_ptr<PrivateAggregationManager> private_aggregation_manager_;

  // The winning buyer's origin. Set to std::nullopt for non-FLEDGE reporter.
  const std::optional<url::Origin> winner_origin_;

  // The aggregation coordinator origin for the winning buyer. Set to
  // std::nullopt for non-FLEDGE reporter or if the default coordinator should
  // be used.
  const std::optional<url::Origin> winner_aggregation_coordinator_origin_;

  // Origins allowed to receive macro expanded reports.
  const std::optional<std::vector<url::Origin>> allowed_reporting_origins_;

  // Whether there has been an attempt to send a custom destination url with
  // macro substitution report to a disallowed origin (according to
  // `allowed_reporting_origins_`). Once this occurs, custom destination url
  // reports will be disabled for the remainder of the FencedFrameReporter's
  // lifetime. This prevents an interest group from encoding cross-site data
  // about a user in binary with its choices of allowed/disallowed origins.
  bool attempted_custom_url_report_to_disallowed_origin_ = false;

  // Private aggregation requests for non-reserved event types registered in
  // bidder worklets, keyed by event type.
  // OnForEventPrivateAggregationRequestsReceived() builds this map up.
  std::map<std::string, PrivateAggregationRequests>
      private_aggregation_event_map_;

  // Fenced frame events for private aggregation API. An event is not removed
  // from the set even after corresponding non-reserved private aggregation
  // requests are sent, because more requests associated with this event might
  // be received and need to be sent later.
  std::set<std::string> received_pa_events_;

  // Which API created this fenced frame reporter instance.
  PrivacySandboxInvokingAPI invoking_api_;

  base::ObserverList<ObserverForTesting> observers_;

  // Tracks the number of beacons sent during the lifetime of the reporter.
  // Logged as a histogram in the destructor.
  unsigned int beacons_sent_same_origin_ = 0;
  unsigned int beacons_sent_cross_origin_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_
