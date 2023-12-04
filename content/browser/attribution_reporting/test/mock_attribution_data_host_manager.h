// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

class MockAttributionDataHostManager : public AttributionDataHostManager {
 public:
  MockAttributionDataHostManager();
  ~MockAttributionDataHostManager() override;

  MOCK_METHOD(
      void,
      RegisterDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       attribution_reporting::SuitableOrigin context_origin,
       bool is_within_fenced_frame,
       attribution_reporting::mojom::RegistrationEligibility,
       GlobalRenderFrameHostId,
       int64_t last_navigation_id),
      (override));

  MOCK_METHOD(
      bool,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token,
       size_t expected_registrations),
      (override));

  MOCK_METHOD(void,
              NotifyNavigationRegistrationStarted,
              (const blink::AttributionSrcToken& attribution_src_token,
               AttributionInputEvent input_event,
               const attribution_reporting::SuitableOrigin& source_origin,
               bool is_within_fenced_frame,
               GlobalRenderFrameHostId,
               int64_t navigation_id,
               std::string devtools_request_id),
              (override));

  MOCK_METHOD(bool,
              NotifyNavigationRegistrationData,
              (const blink::AttributionSrcToken& attribution_src_token,
               const net::HttpResponseHeaders* headers,
               GURL reporting_url,
               network::AttributionReportingRuntimeFeatures),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationRegistrationCompleted,
              (const blink::AttributionSrcToken& attribution_src_token),
              (override));

  MOCK_METHOD(void,
              NotifyBackgroundRegistrationStarted,
              (BackgroundRegistrationsId id,
               const attribution_reporting::SuitableOrigin& context_origin,
               bool is_within_fenced_frame,
               attribution_reporting::mojom::RegistrationEligibility,
               GlobalRenderFrameHostId,
               int64_t last_navigation_id,
               absl::optional<blink::AttributionSrcToken>,
               std::string devtools_request_id),
              (override));

  MOCK_METHOD(bool,
              NotifyBackgroundRegistrationData,
              (BackgroundRegistrationsId id,
               const net::HttpResponseHeaders* headers,
               GURL reporting_url,
               network::AttributionReportingRuntimeFeatures,
               std::vector<network::TriggerVerification>),
              (override));

  MOCK_METHOD(void,
              NotifyBackgroundRegistrationCompleted,
              (BackgroundRegistrationsId id),
              (override));

  MOCK_METHOD(void,
              NotifyFencedFrameReportingBeaconStarted,
              (BeaconId beacon_id,
               absl::optional<int64_t> navigation_id,
               attribution_reporting::SuitableOrigin source_origin,
               bool is_within_fenced_frame,
               AttributionInputEvent input_event,
               GlobalRenderFrameHostId,
               std::string devtools_request_id),
              (override));

  MOCK_METHOD(void,
              NotifyFencedFrameReportingBeaconData,
              (BeaconId beacon_id,
               network::AttributionReportingRuntimeFeatures,
               GURL reporting_url,
               const net::HttpResponseHeaders* headers,
               bool is_final_response),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_
