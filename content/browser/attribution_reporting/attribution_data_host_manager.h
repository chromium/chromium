// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/attribution_reporting/data_host.mojom-forward.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

class AttributionSuitableContext;

// Interface responsible for coordinating `AttributionDataHost`s received from
// the renderer.
class AttributionDataHostManager {
 public:
  virtual ~AttributionDataHostManager() = default;

  // Registers a new data host with the browser process for the given context
  // origin. This is only called for events which are not associated with a
  // navigation.
  virtual void RegisterDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
      AttributionSuitableContext,
      attribution_reporting::mojom::RegistrationEligibility,
      bool is_for_background_requests) = 0;

  // Registers a new data host which is associated with a navigation. The
  // context origin will be provided at a later time in
  // `NotifyNavigationRegistrationStarted()` called with the same
  // `attribution_src_token`. Returns `false` if `attribution_src_token` was
  // already registered.
  virtual bool RegisterNavigationDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) = 0;

  // Notifies the manager that an attribution-enabled navigation associated to
  // the token `attribution_src_token` will start. Alongside the navigation,
  // `background_registrations_count` requests are sent. This method is only
  // called when background requests are sent alongside the navigation. It is
  // guaranteed to be called before `NotifyNavigationRegistrationStarted` with
  // the same `attribution_src_token`. Returns `false` if
  // `attribution_src_token` was already registered or if
  // `expected_registrations` is invalid.
  virtual bool NotifyNavigationWithBackgroundRegistrationsWillStart(
      const blink::AttributionSrcToken& attribution_src_token,
      size_t background_registrations_count) = 0;

  // Notifies the manager that an attribution-enabled navigation has started.
  // This may arrive before or after the attribution configuration is available
  // for a given data host. Every call to `NotifyNavigationRegistrationStarted`
  // must be eventually followed by a call to
  // `NotifyNavigationRegistrationCompleted` with the same
  // `attribution_src_token`.
  virtual void NotifyNavigationRegistrationStarted(
      AttributionSuitableContext suitable_context,
      const blink::AttributionSrcToken& attribution_src_token,
      int64_t navigation_id,
      std::string devtools_request_id) = 0;

  // Notifies the manager that an attribution request tied to an
  // attribution-enabled navigation with token `attribution_src_token` has sent
  // a response. It might be called multiple times for the same navigation; for
  // redirects and a final response. Important: `headers` is untrusted. Must
  // only be called for `attribution_src_token` for which
  // `NotifyNavigationRegistrationStarted` was previously called.
  //
  // Returns true if there was Attribution Reporting relevant response headers.
  virtual bool NotifyNavigationRegistrationData(
      const blink::AttributionSrcToken& attribution_src_token,
      const net::HttpResponseHeaders* headers,
      GURL reporting_url) = 0;

  // Notifies the manager whenever an attribution-enabled navigation request
  // completes. Should be called even for navigations when
  // `NotifyNavigationRegistrationStarted` did not get call for the token as
  // `RegisterNavigationDataHost` might have been called with the token.
  virtual void NotifyNavigationRegistrationCompleted(
      const blink::AttributionSrcToken& attribution_src_token) = 0;

  // Notifies the manager that a background attribution request has started.
  // Every call to `NotifyBackgroundRegistrationStarted` must be eventually
  // followed by a call to `NotifyBackgroundRegistrationCompleted` with the same
  // `id`. If `attribution_src_token` is set, it indicates that the request is
  // tied to a navigation for which the context is provided by a call to
  // `NotifyNavigationRegistrationStarted()` with the same
  // `attribution_src_token`.
  virtual void NotifyBackgroundRegistrationStarted(
      BackgroundRegistrationsId id,
      AttributionSuitableContext,
      attribution_reporting::mojom::RegistrationEligibility,
      std::optional<blink::AttributionSrcToken> attribution_src_token,
      std::optional<std::string> devtools_request_id) = 0;

  // Notifies the manager that a background attribution request has sent a
  // response. May be called multiple times for the same request; for redirects
  // and a final response. Important: `headers` is untrusted.
  //
  // Returns true if there was Attribution Reporting relevant response headers
  // processed.
  virtual bool NotifyBackgroundRegistrationData(
      BackgroundRegistrationsId id,
      const net::HttpResponseHeaders* headers,
      GURL reporting_url) = 0;

  // Notifies the manager that a background attribution request has completed.
  virtual void NotifyBackgroundRegistrationCompleted(
      BackgroundRegistrationsId id) = 0;

  // Notifies the manager that a fenced frame reporting beacon was initiated for
  // reportEvent or for an automatic beacon and should be tracked. The actual
  // beacon may be sent after the navigation finished or after the RFHI was
  // destroyed, therefore we need to store the information for later use.
  // `navigation_id` is the id of the navigation for automatic beacons and
  // `std::nullopt` for event beacons.
  virtual void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      AttributionSuitableContext,
      std::optional<int64_t> navigation_id,
      std::string devtools_request_id) = 0;

  // Notifies the manager whenever a response has been received to a beacon HTTP
  // request. Must be invoked for each redirect received, as well as the final
  // response. `reporting_origin` is the origin that sent `headers` that may
  // contain attribution source registration. `is_final_response` indicates
  // whether this is a redirect or a final response.
  // An opaque origin will be set for `reporting_origin` if the beacon failed to
  // be sent.
  virtual void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      GURL reporting_url,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) = 0;

  // Get a WeakPtr to the implementation instance.
  virtual base::WeakPtr<AttributionDataHostManager> AsWeakPtr() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
