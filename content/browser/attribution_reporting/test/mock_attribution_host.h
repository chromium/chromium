// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_HOST_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_HOST_H_

#include <stdint.h>

#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"

namespace content {

class WebContents;

class MockAttributionHost : public AttributionHost {
 public:
  static MockAttributionHost* Override(WebContents*);

  ~MockAttributionHost() override;

  MOCK_METHOD(void,
              RegisterDataHost,
              (mojo::PendingReceiver<blink::mojom::AttributionDataHost>,
               attribution_reporting::mojom::RegistrationEligibility),
              (override));

  MOCK_METHOD(void,
              RegisterNavigationDataHost,
              (mojo::PendingReceiver<blink::mojom::AttributionDataHost>,
               const blink::AttributionSrcToken&,
               uint32_t expected_registrations),
              (override));

 private:
  explicit MockAttributionHost(WebContents*);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_TEST_MOCK_ATTRIBUTION_HOST_H_
