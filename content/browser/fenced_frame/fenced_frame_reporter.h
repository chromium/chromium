// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Class that receives report events from fenced frames, and uses a
// per-destination-type maps of events to URLs to send reports. The maps may be
// received after the report event calls, in which case the reports will be
// queued until the corresponding map types have been received.
class CONTENT_EXPORT FencedFrameReporter
    : public base::RefCounted<FencedFrameReporter> {
 public:
  using ReportingUrlMap = base::flat_map<std::string, GURL>;

  FencedFrameReporter(
      base::PassKey<FencedFrameReporter> pass_key,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Creates a FencedFrameReporter that only maps kSharedStorageSelectUrl
  // destinations, using the passed in map.
  //
  // `url_loader_factory` is used to send all reports, and must not be null.
  static scoped_refptr<FencedFrameReporter> CreateForSharedStorage(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ReportingUrlMap reporting_url_map);

  // Creates a FencedFrameReporter that maps FLEDGE ReportingDestination types
  // (kBuyer, kSeller, kComponentSeller), but that initially considers all three
  // map types pending, and just collects reporting strings of those types until
  // the corresponding mappings are passed in via OnUrlMappingReady().
  //
  // `url_loader_factory` is used to send all reports, and must not be null.
  static scoped_refptr<FencedFrameReporter> CreateForFledge(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Called when a mapping for reports of type `reporting_destination` is ready.
  // The reporter must currently be considering maps of type
  // `reporting_destination` pending - that is:
  //
  // 1) It must have been created by CreateForFledge()
  // 2) `reporting_destination` must be one of kBuyer, kSeller, or
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
  // TODO(https://crbug.com/1409133): Consider investing in outputting error to
  // correct frame, if it still exists. `frame_tree_node_id` somewhat does this,
  // though it doesn't change across navigations, so could end up displaying an
  // error for a page a frame was previously displaying. There may be other
  // options.
  void OnUrlMappingReady(
      blink::FencedFrame::ReportingDestination reporting_destination,
      ReportingUrlMap reporting_url_map);

  // Uses `event_type`, `event_data` and the ReportingUrlMap associated with
  // `reporting_destination` to send a report. If the map for
  // `reporting_destination` is pending, queues the report until the mapping
  // information is received. If there's no matching information for
  // `event_type`, does nothing.
  //
  // Returns false and populated `error_message` if no network request was
  // attempted, unless the reporting URL map for `reporting_destination` is
  // pending. In that case, errors are currently never reported, even if the
  // reporting URL map results in no request being sent.
  bool SendReport(
      const std::string& event_type,
      const std::string& event_data,
      blink::FencedFrame::ReportingDestination reporting_destination,
      const url::Origin& request_initiator,
      std::string& error_message);

 private:
  friend class base::RefCounted<FencedFrameReporter>;
  friend class FencedFrameURLMappingTestPeer;

  struct PendingEvent {
    PendingEvent(const std::string& type,
                 const std::string& data,
                 const url::Origin& request_initiator);

    std::string type;
    std::string data;
    url::Origin request_initiator;
  };

  // The per-blink::FencedFrame::ReportingDestination reporting information.
  struct ReportingDestinationInfo {
    explicit ReportingDestinationInfo(
        absl::optional<ReportingUrlMap> reporting_url_map = absl::nullopt);
    ReportingDestinationInfo(ReportingDestinationInfo&&);
    ~ReportingDestinationInfo();

    ReportingDestinationInfo& operator=(ReportingDestinationInfo&&);

    // If null, the reporting URL map has yet to be received, and any reports
    // that are attempted to be sent of the corresponding type will be added to
    // `pending_events`, and only sent once this is populated.
    absl::optional<ReportingUrlMap> reporting_url_map;

    // Pending report strings received while `reporting_url_map` was
    // absl::nullopt. Once the map is received, this is cleared, and reports are
    // sent.
    std::vector<PendingEvent> pending_events;
  };

  ~FencedFrameReporter();

  // Helper to send a report, used by both SendReport() and OnUrlMappingReady().
  bool SendReportInternal(
      const ReportingDestinationInfo& reporting_destination_info,
      const std::string& event_type,
      const std::string& event_data,
      blink::FencedFrame::ReportingDestination reporting_destination,
      const url::Origin& request_initiator,
      std::string& error_message);

  // Used by FencedFrameURLMappingTestPeer.
  const base::flat_map<blink::FencedFrame::ReportingDestination,
                       ReportingDestinationInfo>&
  reporting_metadata() const {
    return reporting_metadata_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::flat_map<blink::FencedFrame::ReportingDestination,
                 ReportingDestinationInfo>
      reporting_metadata_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_REPORTER_H_
