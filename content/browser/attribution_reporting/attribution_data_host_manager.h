// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <stdint.h>

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/attribution_reporting/registration_type.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"
#include "third_party/blink/public/mojom/conversions/attribution_reporting.mojom-forward.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content {

class BrowserContext;

struct AttributionInputEvent;
struct GlobalRenderFrameHostId;

// Interface responsible for coordinating `AttributionDataHost`s received from
// the renderer.
class CONTENT_EXPORT AttributionDataHostManager
    : public base::SupportsWeakPtr<AttributionDataHostManager> {
 public:
  static AttributionDataHostManager* FromBrowserContext(BrowserContext*);

  AttributionDataHostManager();
  virtual ~AttributionDataHostManager();

  // Registers a new data host with the browser process for the given context
  // origin. This is only called for events which are not associated with a
  // navigation. Passes the topmost ancestor of the initiator render frame for
  // obtaining the page access report.
  virtual void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      attribution_reporting::SuitableOrigin context_origin,
      bool is_within_fenced_frame,
      attribution_reporting::mojom::RegistrationType,
      GlobalRenderFrameHostId render_frame_id) = 0;

  // Registers a new data host which is associated with a navigation. The
  // context origin will be provided at a later time in
  // `NotifyNavigationForDataHost()` called with the same
  // `attribution_src_token`. Returns `false` if `attribution_src_token` was
  // already registered.
  virtual bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token,
      AttributionInputEvent input_event) = 0;

  // Notifies the manager that an attribution enabled navigation has registered
  // a source header. May be called multiple times for the same navigation.
  // Important: `header_value` is untrusted. Passes the topmost ancestor of the
  // initiator render frame for obtaining the page access report.
  virtual void NotifyNavigationRedirectRegistration(
      const blink::AttributionSrcToken& attribution_src_token,
      std::string header_value,
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::SuitableOrigin& source_origin,
      AttributionInputEvent input_event,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id) = 0;

  // Notifies the manager that we have received a navigation for a given data
  // host. This may arrive before or after the attribution configuration is
  // available for a given data host. Passes the topmost ancestor of the
  // initiator render frame for obtaining the page access report.
  virtual void NotifyNavigationForDataHost(
      const blink::AttributionSrcToken& attribution_src_token,
      const attribution_reporting::SuitableOrigin& source_origin,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id) = 0;

  // Notifies the manager that a navigation failed and should no longer be
  // tracked. The navigation was associated with a data host if
  // `attribution_src_token` is not `absl::nullopt`.
  virtual void NotifyNavigationFailure(
      const absl::optional<blink::AttributionSrcToken>& attribution_src_token,
      int64_t navigation_id) = 0;

  // Notifies the manager that a navigation finished. This may arrive before or
  // after the beacon data.
  virtual void NotifyNavigationSuccess(int64_t navigation_id) = 0;

  // Notifies the manager that a fenced frame reporting beacon was initiated
  // for reportEvent or for an automatic beacon and should be tracked.
  // The actual beacon may be sent after the navigation finished or after the
  // RFHI was destroyed, therefore we need to store the information for later
  // use. Passes the topmost ancestor of the initiator render frame for
  // obtaining the page access report.
  virtual void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      attribution_reporting::SuitableOrigin source_origin,
      bool is_within_fenced_frame,
      absl::optional<AttributionInputEvent> input_event,
      GlobalRenderFrameHostId render_frame_id) = 0;

  // Notifies the manager that a beacon has been sent.
  virtual void NotifyFencedFrameReportingBeaconSent(BeaconId beacon_id) = 0;

  // Notifies the manager whenever a response has been received to a beacon HTTP
  // request. Must be invoked for each redirect received, as well as the final
  // response. `reporting_origin` is the origin that sent `headers` that may
  // contain attribution source registration. `is_final_response` indicates
  // whether this is a redirect or a final response.
  virtual void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      url::Origin reporting_origin,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
