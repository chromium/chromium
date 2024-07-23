// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/attribution_reporting/data_host.mojom-forward.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

class MockAttributionDataHostManager final : public AttributionDataHostManager {
 public:
  MockAttributionDataHostManager();
  ~MockAttributionDataHostManager() override;

  MOCK_METHOD(
      void,
      RegisterDataHost,
      (mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
       AttributionSuitableContext,
       attribution_reporting::mojom::RegistrationEligibility,
       bool),
      (override));

  MOCK_METHOD(
      bool,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token),
      (override));

  MOCK_METHOD(bool,
              NotifyNavigationWithBackgroundRegistrationsWillStart,
              (const blink::AttributionSrcToken& attribution_src_token,
               size_t expected_registrations),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationRegistrationStarted,
              (AttributionSuitableContext suitable_context,
               const blink::AttributionSrcToken& attribution_src_token,
               int64_t navigation_id,
               std::string devtools_request_id),
              (override));

  MOCK_METHOD(bool,
              NotifyNavigationRegistrationData,
              (const blink::AttributionSrcToken& attribution_src_token,
               const net::HttpResponseHeaders* headers,
               GURL reporting_url),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationRegistrationCompleted,
              (const blink::AttributionSrcToken& attribution_src_token),
              (override));

  MOCK_METHOD(void,
              NotifyBackgroundRegistrationStarted,
              (BackgroundRegistrationsId id,
               AttributionSuitableContext,
               attribution_reporting::mojom::RegistrationEligibility,
               std::optional<blink::AttributionSrcToken>,
               std::optional<std::string> devtools_request_id),
              (override));

  MOCK_METHOD(bool,
              NotifyBackgroundRegistrationData,
              (BackgroundRegistrationsId id,
               const net::HttpResponseHeaders* headers,
               GURL reporting_url),
              (override));

  MOCK_METHOD(void,
              NotifyBackgroundRegistrationCompleted,
              (BackgroundRegistrationsId id),
              (override));

  MOCK_METHOD(void,
              NotifyFencedFrameReportingBeaconStarted,
              (BeaconId beacon_id,
               AttributionSuitableContext suitable_context,
               std::optional<int64_t> navigation_id,
               std::string devtools_request_id),
              (override));

  MOCK_METHOD(void,
              NotifyFencedFrameReportingBeaconData,
              (BeaconId beacon_id,
               GURL reporting_url,
               const net::HttpResponseHeaders* headers,
               bool is_final_response),
              (override));

  base::WeakPtr<AttributionDataHostManager> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockAttributionDataHostManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_DATA_HOST_MANAGER_H_
