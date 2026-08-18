// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_keys_fetcher.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version_info/channel.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

constexpr char kTestAccountEmail[] = "test@example.com";
constexpr char kFakeResponseBody[] = R"({"deviceAuthorizationKeys": {}})";

class DeviceAuthorizationKeysFetcherTest : public testing::Test {
 protected:
  DeviceAuthorizationKeysFetcherTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    identity_test_environment_.MakePrimaryAccountAvailable(
        kTestAccountEmail, signin::ConsentLevel::kSignin);
    identity_test_environment_.SetAutomaticIssueOfAccessTokens(true);
  }
  ~DeviceAuthorizationKeysFetcherTest() override = default;

  // Synchronously fetches device authorization keys using `fetcher_`,
  // responding with `status_code` and `response_body`.
  std::unique_ptr<endpoint_fetcher::EndpointResponse>
  FetchDeviceAuthorizationKeys(net::HttpStatusCode status_code,
                               const std::string& response_body) {
    test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                         response_body, status_code);
    base::test::TestFuture<std::unique_ptr<endpoint_fetcher::EndpointResponse>>
        future;
    fetcher_.FetchDeviceAuthorizationKeys(
        shared_url_loader_factory_,
        identity_test_environment_.identity_manager(), future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  DeviceAuthorizationKeysFetcher fetcher_{version_info::Channel::DEFAULT};
};

// Tests that device authorization keys request handles failure to fetch an
// access token.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ShouldReturnErrorWhenAccessTokenFetchFails) {
  identity_test_environment_.SetAutomaticIssueOfAccessTokens(false);
  base::test::TestFuture<std::unique_ptr<endpoint_fetcher::EndpointResponse>>
      future;
  fetcher_.FetchDeviceAuthorizationKeys(
      shared_url_loader_factory_, identity_test_environment_.identity_manager(),
      future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError::FromServiceError(""));

  std::unique_ptr<endpoint_fetcher::EndpointResponse> response = future.Take();
  ASSERT_TRUE(response);
  EXPECT_TRUE(response->error_type.has_value());
}

// Tests that device authorization keys request handles HTTP server errors.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ShouldReturnErrorWhenHttpErrorOccurs) {
  std::unique_ptr<endpoint_fetcher::EndpointResponse> response =
      FetchDeviceAuthorizationKeys(net::HTTP_INTERNAL_SERVER_ERROR, "");
  ASSERT_TRUE(response);
  EXPECT_EQ(response->http_status_code, net::HTTP_INTERNAL_SERVER_ERROR);
}

// Tests that device authorization keys request completes successfully with a
// valid response.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ShouldSuccessfullyFetchDeviceAuthorizationKeys) {
  std::unique_ptr<endpoint_fetcher::EndpointResponse> response =
      FetchDeviceAuthorizationKeys(net::HTTP_OK, kFakeResponseBody);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->http_status_code, net::HTTP_OK);
  EXPECT_EQ(response->response, kFakeResponseBody);
}

// Tests that calling fetch while another fetch is in progress immediately
// completes the second request with nullptr.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ShouldReturnNullptrWhenFetchAlreadyInProgress) {
  base::test::TestFuture<std::unique_ptr<endpoint_fetcher::EndpointResponse>>
      future_first;
  fetcher_.FetchDeviceAuthorizationKeys(
      shared_url_loader_factory_, identity_test_environment_.identity_manager(),
      future_first.GetCallback());

  base::test::TestFuture<std::unique_ptr<endpoint_fetcher::EndpointResponse>>
      future_second;
  fetcher_.FetchDeviceAuthorizationKeys(
      shared_url_loader_factory_, identity_test_environment_.identity_manager(),
      future_second.GetCallback());

  // The second request should immediately return nullptr.
  EXPECT_EQ(future_second.Take(), nullptr);

  // Complete the first request.
  test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                       kFakeResponseBody, net::HTTP_OK);
  std::unique_ptr<endpoint_fetcher::EndpointResponse> response_first =
      future_first.Take();
  ASSERT_TRUE(response_first);
  EXPECT_EQ(response_first->http_status_code, net::HTTP_OK);
}

}  // namespace
}  // namespace webauthn
