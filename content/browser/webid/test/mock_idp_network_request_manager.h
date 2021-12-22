// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_

#include "content/browser/webid/idp_network_request_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockIdpNetworkRequestManager : public IdpNetworkRequestManager {
 public:
  MockIdpNetworkRequestManager(const GURL& provider,
                               const url::Origin& relaying_party_origin);

  ~MockIdpNetworkRequestManager() override;

  MockIdpNetworkRequestManager(const MockIdpNetworkRequestManager&) = delete;
  MockIdpNetworkRequestManager& operator=(const MockIdpNetworkRequestManager&) =
      delete;

  MOCK_METHOD1(FetchIdpWellKnown, void(FetchWellKnownCallback));
  MOCK_METHOD3(FetchClientIdMetadata,
               void(const GURL&,
                    const std::string&,
                    FetchClientIdMetadataCallback));
  MOCK_METHOD3(SendSigninRequest,
               void(const GURL&, const std::string&, SigninRequestCallback));
  MOCK_METHOD5(SendAccountsRequest,
               void(const GURL&,
                    int,
                    int,
                    BrandIconDownloader,
                    AccountsRequestCallback));
  MOCK_METHOD4(SendTokenRequest,
               void(const GURL&,
                    const std::string&,
                    const std::string&,
                    TokenRequestCallback));
  MOCK_METHOD2(SendLogout, void(const GURL& logout_url, LogoutCallback));
  MOCK_METHOD4(SendRevokeRequest,
               void(const GURL&,
                    const std::string&,
                    const std::string&,
                    RevokeCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
