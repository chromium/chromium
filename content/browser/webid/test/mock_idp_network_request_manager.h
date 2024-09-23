// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_

#include "content/browser/webid/idp_network_request_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class MockIdpNetworkRequestManager : public IdpNetworkRequestManager {
 public:
  MockIdpNetworkRequestManager();
  ~MockIdpNetworkRequestManager() override;

  MockIdpNetworkRequestManager(const MockIdpNetworkRequestManager&) = delete;
  MockIdpNetworkRequestManager& operator=(const MockIdpNetworkRequestManager&) =
      delete;

  MOCK_METHOD(void,
              FetchWellKnown,
              (const GURL&, FetchWellKnownCallback),
              (override));
  MOCK_METHOD(
      void,
      FetchConfig,
      (const GURL&, blink::mojom::RpMode, int, int, FetchConfigCallback),
      (override));
  MOCK_METHOD(
      void,
      FetchClientMetadata,
      (const GURL&, const std::string&, int, int, FetchClientMetadataCallback),
      (override));
  MOCK_METHOD(void,
              SendAccountsRequest,
              (const GURL&, const std::string&, AccountsRequestCallback),
              (override));
  MOCK_METHOD(void,
              SendTokenRequest,
              (const GURL&,
               const std::string&,
               const std::string&,
               TokenRequestCallback,
               ContinueOnCallback,
               RecordErrorMetricsCallback),
              (override));
  MOCK_METHOD(void,
              SendSuccessfulTokenRequestMetrics,
              (const GURL&,
               base::TimeDelta,
               base::TimeDelta,
               base::TimeDelta,
               base::TimeDelta),
              (override));
  MOCK_METHOD(void,
              SendFailedTokenRequestMetrics,
              (const GURL&, MetricsEndpointErrorCode code),
              (override));
  MOCK_METHOD(void,
              SendLogout,
              (const GURL& logout_url, LogoutCallback),
              (override));
  MOCK_METHOD(void,
              DownloadAndDecodeImage,
              (const GURL&, ImageCallback),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
