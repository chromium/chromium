// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_keys_fetcher.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_metrics.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_switches.h"
#include "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

constexpr char kTestAccountEmail[] = "test@example.com";
constexpr char kTestKey[] = "test_device_authorization_key_material";
constexpr char kTestPlt[] = "test_plt_token";
constexpr char kTestWebFallbackUrl[] = "https://example.com/reauth";

using Error = DeviceAuthorizationKeysFetcher::Error;

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
  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>
  FetchDeviceAuthorizationKeys(
      const sync_pb::GetDeviceAuthorizationKeyRequest& request,
      net::HttpStatusCode status_code,
      const std::string& response_body) {
    test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                         response_body, status_code);
    base::test::TestFuture<
        base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>>
        future;
    fetcher_.FetchDeviceAuthorizationKeys(
        request, shared_url_loader_factory_,
        identity_test_environment_.identity_manager(), future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  DeviceAuthorizationKeysFetcher fetcher_{version_info::Channel::DEFAULT};
};

// Tests that device authorization keys request completes successfully when
// the backend returns device authorization keys.
TEST_F(DeviceAuthorizationKeysFetcherTest, ReturnsDeviceAuthorizationKeys) {
  base::HistogramTester histogram_tester;
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  request.set_reauth_proof_token("sample_rapt");

  std::string intercepted_body;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        intercepted_body = network::GetUploadData(req);
      }));

  sync_pb::GetDeviceAuthorizationKeyResponse response_proto;
  sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys*
      keys_proto = response_proto.mutable_device_authorization_keys();
  sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKey* key =
      keys_proto->add_keys();
  key->set_version(1);
  key->set_key(kTestKey);

  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
      FetchDeviceAuthorizationKeys(request, net::HTTP_OK,
                                   response_proto.SerializeAsString());

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->has_device_authorization_keys());
  ASSERT_EQ(result->device_authorization_keys().keys_size(), 1);
  EXPECT_EQ(result->device_authorization_keys().keys(0).version(), 1);
  EXPECT_EQ(result->device_authorization_keys().keys(0).key(), kTestKey);

  // Verify the request body sent matches serialized proto.
  EXPECT_EQ(intercepted_body, request.SerializeAsString());

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kKeysFetched, 1);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      net::HTTP_OK, 1);
}

// Tests that device authorization keys request completes successfully when
// the backend returns re-auth parameters.
TEST_F(DeviceAuthorizationKeysFetcherTest, ReturnsReAuthParams) {
  base::HistogramTester histogram_tester;
  sync_pb::GetDeviceAuthorizationKeyRequest request;

  sync_pb::GetDeviceAuthorizationKeyResponse response_proto;
  sync_pb::GetDeviceAuthorizationKeyResponse::ReAuthParams* re_auth_params =
      response_proto.mutable_re_auth_params();
  re_auth_params->set_plt(kTestPlt);
  re_auth_params->set_web_fallback_url(kTestWebFallbackUrl);

  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
      FetchDeviceAuthorizationKeys(request, net::HTTP_OK,
                                   response_proto.SerializeAsString());

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->has_re_auth_params());
  EXPECT_EQ(result->re_auth_params().plt(), kTestPlt);
  EXPECT_EQ(result->re_auth_params().web_fallback_url(), kTestWebFallbackUrl);

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kReAuthChallenge, 1);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      net::HTTP_OK, 1);
}

// Tests that device authorization keys request handles failure to fetch an
// access token.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ReturnsErrorWhenAccessTokenFetchFails) {
  base::HistogramTester histogram_tester;
  identity_test_environment_.SetAutomaticIssueOfAccessTokens(false);
  sync_pb::GetDeviceAuthorizationKeyRequest request;

  base::test::TestFuture<
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>>
      future;
  fetcher_.FetchDeviceAuthorizationKeys(
      request, shared_url_loader_factory_,
      identity_test_environment_.identity_manager(), future.GetCallback());
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError::FromServiceError(""));

  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::kNetworkError);

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kNetworkError, 1);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      net::ERR_FAILED, 1);
}

// Tests that device authorization keys request handles transport/network
// errors.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ReturnsNetworkErrorWhenTransportFails) {
  base::HistogramTester histogram_tester;
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  test_url_loader_factory_.AddResponse(
      GURL(kDeviceAuthorizationKeyEndpointUrl),
      network::CreateURLResponseHead(net::HTTP_OK), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_FAILED));

  base::test::TestFuture<
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>>
      future;
  fetcher_.FetchDeviceAuthorizationKeys(
      request, shared_url_loader_factory_,
      identity_test_environment_.identity_manager(), future.GetCallback());

  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
      future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::kNetworkError);

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kNetworkError, 1);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      net::ERR_FAILED, 1);
}

// Tests that device authorization keys request handles HTTP server errors.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ReturnsHttpErrorWhenHttpErrorOccurs) {
  sync_pb::GetDeviceAuthorizationKeyRequest request;

  for (net::HttpStatusCode status : {net::HTTP_BAD_REQUEST, net::HTTP_FORBIDDEN,
                                     net::HTTP_INTERNAL_SERVER_ERROR}) {
    base::HistogramTester histogram_tester;
    base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
        FetchDeviceAuthorizationKeys(request, status, "");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Error::kHttpError);

    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.DeviceAuthorization.FetchResult",
        DeviceAuthorizationFetchResultForUMA::kHttpError, 1);
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError", status,
        1);

    test_url_loader_factory_.ClearResponses();
  }
}

// Tests that device authorization keys request handles corrupt or invalid
// response protobuf body.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ReturnsProtoParseErrorWhenResponseCorrupt) {
  base::HistogramTester histogram_tester;
  sync_pb::GetDeviceAuthorizationKeyRequest request;
  // An invalid protobuf payload that cannot be deserialized as
  // GetDeviceAuthorizationKeyResponse.
  const std::string kCorruptProto = "\xff\xff\xff\xff";

  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error> result =
      FetchDeviceAuthorizationKeys(request, net::HTTP_OK, kCorruptProto);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Error::kProtoParseError);

  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kProtoParseError, 1);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.DeviceAuthorization.HttpStatusOrNetError",
      net::HTTP_OK, 1);
}

// Tests that calling fetch while another fetch is in progress immediately
// completes the second request with kAlreadyInProgress.
TEST_F(DeviceAuthorizationKeysFetcherTest,
       ReturnsAlreadyInProgressWhenFetchConcurrent) {
  base::HistogramTester histogram_tester;
  sync_pb::GetDeviceAuthorizationKeyRequest request;

  base::test::TestFuture<
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>>
      future_first;
  fetcher_.FetchDeviceAuthorizationKeys(
      request, shared_url_loader_factory_,
      identity_test_environment_.identity_manager(),
      future_first.GetCallback());

  base::test::TestFuture<
      base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>>
      future_second;
  fetcher_.FetchDeviceAuthorizationKeys(
      request, shared_url_loader_factory_,
      identity_test_environment_.identity_manager(),
      future_second.GetCallback());

  // The second request should immediately return kAlreadyInProgress.
  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>
      result_second = future_second.Take();
  ASSERT_FALSE(result_second.has_value());
  EXPECT_EQ(result_second.error(), Error::kAlreadyInProgress);

  histogram_tester.ExpectBucketCount(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kAlreadyInProgress, 1);

  // Complete the first request.
  sync_pb::GetDeviceAuthorizationKeyResponse response_proto;
  sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys* keys =
      response_proto.mutable_device_authorization_keys();
  keys->add_keys()->set_version(1);

  test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                       response_proto.SerializeAsString(),
                                       net::HTTP_OK);
  base::expected<sync_pb::GetDeviceAuthorizationKeyResponse, Error>
      result_first = future_first.Take();
  ASSERT_TRUE(result_first.has_value());
  EXPECT_TRUE(result_first->has_device_authorization_keys());

  histogram_tester.ExpectBucketCount(
      "WebAuthentication.DeviceAuthorization.FetchResult",
      DeviceAuthorizationFetchResultForUMA::kKeysFetched, 1);
}

}  // namespace
}  // namespace webauthn
