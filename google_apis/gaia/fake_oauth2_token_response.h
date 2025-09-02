// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_RESPONSE_H_
#define GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_RESPONSE_H_

#include <optional>
#include <string>
#include <variant>

#include "base/time/time.h"
#include "google_apis/gaia/oauth2_response.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace gaia {

// A builder to fake OAuth2 token responses for tests.
// This allows tests to specify their intent (e.g., success, specific error)
// without needing to know the exact JSON format.
class FakeOAuth2TokenResponse {
 public:
  // Specifies the target API endpoint to fake. These endpoints are not fully
  // equivalent and they use different request/response formats.
  enum ApiEndpoint {
    kGetToken,    // GaiaUrls::oauth2_token_url()
    kIssueToken,  // GaiaUrls::oauth2_issue_token_url()
  };

  // Creates a response for a successful token fetch.
  static FakeOAuth2TokenResponse Success(
      std::string_view access_token,
      base::TimeDelta expires_in = base::Hours(1));

  // Creates a response for a standard OAuth2 error. `http_status_code_override`
  // is used to override the default HTTP status code for the given error.
  static FakeOAuth2TokenResponse OAuth2Error(
      OAuth2Response error,
      std::optional<net::HttpStatusCode> http_status_code_override =
          std::nullopt);

  // Creates a response with a specific net error code.
  static FakeOAuth2TokenResponse NetError(net::Error net_error);

  FakeOAuth2TokenResponse(const FakeOAuth2TokenResponse&);
  FakeOAuth2TokenResponse& operator=(const FakeOAuth2TokenResponse&);
  ~FakeOAuth2TokenResponse();

  // Convenience method to add fake response to `test_url_loader_factory`.
  // `endpoint` specifies the target API endpoint. If `endpoint` is null, all
  // avaiable endpoints will be faked.
  void AddToTestURLLoaderFactory(
      network::TestURLLoaderFactory& test_url_loader_factory,
      std::optional<ApiEndpoint> exclusive_endpoint = std::nullopt) const;

  // `ApiEndpoint` determines which API endpoint to fake.
  std::string GetBody(ApiEndpoint endpoint) const;
  net::HttpStatusCode GetHttpStatusCode(ApiEndpoint endpoint) const;
  net::Error GetNetError(ApiEndpoint endpoint) const;
  GURL GetUrl(ApiEndpoint endpoint) const;

 private:
  struct SuccessData {
    std::string access_token;
    base::TimeDelta expires_in;
  };
  struct ErrorData {
    OAuth2Response error;
    std::optional<net::HttpStatusCode> http_status_code_override;
  };

  using ResponseData = std::variant<SuccessData, ErrorData, net::Error>;
  explicit FakeOAuth2TokenResponse(ResponseData data);

  ResponseData data_;
};

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_FAKE_OAUTH2_TOKEN_RESPONSE_H_
