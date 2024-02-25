// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_DOCUMENT_DATA_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_DOCUMENT_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "content/browser/fenced_frame/automatic_beacon_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"

namespace content {

// Used on the browser-side to store information related to fenced frames and
// URN iframes created using an API like Protected Audience or Shared Storage.
class CONTENT_EXPORT FencedDocumentData
    : public DocumentUserData<FencedDocumentData> {
 public:
  ~FencedDocumentData() override;

  // Attempts to retrieve the automatic beacon data for a given event type.
  const std::optional<AutomaticBeaconInfo> GetAutomaticBeaconInfo(
      blink::mojom::AutomaticBeaconType event_type) const;

  // Writes the beacon data set in `setReportEventDataForAutomaticBeacons()`.
  void UpdateAutomaticBeaconData(
      blink::mojom::AutomaticBeaconType event_type,
      const std::string& event_data,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      bool once,
      bool cross_origin_exposed);

  // Automatic beacon data is cleared out after one automatic beacon if `once`
  // was set to true when calling `setReportEventDataForAutomaticBeacons()`.
  void MaybeResetAutomaticBeaconData(
      blink::mojom::AutomaticBeaconType event_type);

  network::AttributionReportingRuntimeFeatures features() const {
    return features_;
  }

  void SetFeatures(network::AttributionReportingRuntimeFeatures features) {
    features_ = features;
  }

 private:
  // No public constructors to force going through static methods of
  // DocumentUserData (e.g. CreateForCurrentDocument).
  explicit FencedDocumentData(RenderFrameHost* rfh);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  network::AttributionReportingRuntimeFeatures features_;

  // Stores data registered by the document in a fenced frame tree using
  // the `fence.setReportEventDataForAutomaticBeacons` API. Maps an event type
  // to an AutomaticBeaconInfo object.
  std::map<blink::mojom::AutomaticBeaconType, AutomaticBeaconInfo>
      automatic_beacon_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_DOCUMENT_DATA_H_
