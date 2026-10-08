// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/cryptauth_cmtg_device_key_provider.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webauthn {
namespace {

using KeysFuture =
    base::test::TestFuture<base::expected<std::vector<std::vector<uint8_t>>,
                                          CmtgDeviceKeyProvider::Error>>;

constexpr char kGetOrCreateUrl[] =
    "https://cryptauthfidoenrollment.pa.googleapis.com/v1/users/me/"
    "cmtgWrapperKeys:getOrCreate?alt=json";

constexpr char kKeyMaterial[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

constexpr char kGetOrCreateResponse[] = R"({
  "cmtgWrapperKeyInfo": {
    "keyMaterial": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
    "keyAlgorithm": "AES256_GCM",
    "createTime": "2026-10-02T16:03:33.021202Z"
  }
})";

class CryptauthCmtgDeviceKeyProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable(
        "marisa@gmail.com", signin::ConsentLevel::kSignin);
    provider_ = std::make_unique<CryptauthCmtgDeviceKeyProvider>(
        *identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper());
  }

  // Starts a request for `operation`, responds to the OAuth token request and
  // returns the resulting pending network request.
  const network::ResourceRequest& StartRequest(
      CmtgDeviceKeyProvider::Operation operation,
      KeysFuture& future) {
    request_ = provider_->GetDeviceKeys(operation, future.GetCallback());
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "test_access_token", base::Time::Max());
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
    return test_url_loader_factory_.GetPendingRequest(0)->request;
  }

  // Runs a MakeCredential request that receives `response` and returns the
  // result.
  base::expected<std::vector<std::vector<uint8_t>>,
                 CmtgDeviceKeyProvider::Error>
  MakeCredentialWithResponse(const std::string& response) {
    KeysFuture future;
    StartRequest(CmtgDeviceKeyProvider::Operation::kMakeCredential, future);
    test_url_loader_factory_.SimulateResponseForPendingRequest(kGetOrCreateUrl,
                                                               response);
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<CryptauthCmtgDeviceKeyProvider> provider_;
  std::unique_ptr<CmtgDeviceKeyProvider::Request> request_;
};

TEST_F(CryptauthCmtgDeviceKeyProviderTest, MakeCredentialSuccess) {
  base::HistogramTester histogram_tester;
  KeysFuture future;

  const network::ResourceRequest& resource_request =
      StartRequest(CmtgDeviceKeyProvider::Operation::kMakeCredential, future);
  EXPECT_EQ(resource_request.url, GURL(kGetOrCreateUrl));
  EXPECT_EQ(resource_request.method, net::HttpRequestHeaders::kPostMethod);
  EXPECT_EQ(resource_request.headers.GetHeader(
                net::HttpRequestHeaders::kAuthorization),
            "Bearer test_access_token");
  EXPECT_EQ(
      resource_request.headers.GetHeader(net::HttpRequestHeaders::kContentType),
      "application/json");
  EXPECT_EQ(network::GetUploadData(resource_request), "{}");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kGetOrCreateUrl, kGetOrCreateResponse);

  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get()->size(), 1u);
  EXPECT_EQ(future.Get()->front(), *base::Base64Decode(kKeyMaterial));
  histogram_tester.ExpectUniqueSample("WebAuthentication.CmtgDeviceKeys.Result",
                                      CmtgDeviceKeysResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "WebAuthentication.CmtgDeviceKeys.RequestDuration", 1);
}

TEST_F(CryptauthCmtgDeviceKeyProviderTest, MakeCredentialInvalidResponses) {
  constexpr const char* kInvalidResponses[] = {
      // Not JSON.
      "not json",
      // Not a dictionary.
      "[]",
      // Missing key info.
      "{}",
      // Key info is not a dictionary.
      R"({"cmtgWrapperKeyInfo": "foo"})",
      // Missing key material.
      R"({"cmtgWrapperKeyInfo": {"keyAlgorithm": "AES256_GCM"}})",
      // Invalid base64.
      R"({"cmtgWrapperKeyInfo": {"keyMaterial": "!!!",
                                 "keyAlgorithm": "AES256_GCM"}})",
      // Key is too short.
      R"({"cmtgWrapperKeyInfo": {"keyMaterial": "AAAA",
                                 "keyAlgorithm": "AES256_GCM"}})",
      // Missing algorithm.
      R"({"cmtgWrapperKeyInfo": {
          "keyMaterial": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="}})",
      // Unsupported algorithm.
      R"({"cmtgWrapperKeyInfo": {
          "keyMaterial": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
          "keyAlgorithm": "KEY_ALGORITHM_UNSPECIFIED"}})",
  };
  for (const char* response : kInvalidResponses) {
    SCOPED_TRACE(response);
    base::HistogramTester histogram_tester;
    auto result = MakeCredentialWithResponse(response);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CmtgDeviceKeyProvider::Error::kParseError);
    histogram_tester.ExpectUniqueSample(
        "WebAuthentication.CmtgDeviceKeys.Result",
        CmtgDeviceKeysResult::kParseError, 1);
  }
}

// TODO(crbug.com/485888879): Replace with real tests once GetAssertion uses
// the batchGet endpoint.
TEST_F(CryptauthCmtgDeviceKeyProviderTest, GetAssertionReturnsMockKey) {
  KeysFuture future;
  auto request = provider_->GetDeviceKeys(
      CmtgDeviceKeyProvider::Operation::kGetAssertion, future.GetCallback());
  ASSERT_TRUE(future.Get().has_value());
  ASSERT_EQ(future.Get()->size(), 1u);
  EXPECT_EQ(future.Get()->front().size(), 32u);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(CryptauthCmtgDeviceKeyProviderTest, AccessTokenFailure) {
  base::HistogramTester histogram_tester;
  KeysFuture future;

  auto request = provider_->GetDeviceKeys(
      CmtgDeviceKeyProvider::Operation::kMakeCredential, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(),
            CmtgDeviceKeyProvider::Error::kAccessTokenError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  histogram_tester.ExpectUniqueSample("WebAuthentication.CmtgDeviceKeys.Result",
                                      CmtgDeviceKeysResult::kAccessTokenError,
                                      1);
}

TEST_F(CryptauthCmtgDeviceKeyProviderTest, HttpError) {
  base::HistogramTester histogram_tester;
  KeysFuture future;

  StartRequest(CmtgDeviceKeyProvider::Operation::kMakeCredential, future);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kGetOrCreateUrl, kGetOrCreateResponse, net::HTTP_INTERNAL_SERVER_ERROR);

  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(future.Get().error(), CmtgDeviceKeyProvider::Error::kNetworkError);
  histogram_tester.ExpectUniqueSample("WebAuthentication.CmtgDeviceKeys.Result",
                                      CmtgDeviceKeysResult::kNetworkError, 1);
}

TEST_F(CryptauthCmtgDeviceKeyProviderTest, CancelsWhenRequestDestroyed) {
  base::HistogramTester histogram_tester;
  {
    auto request = provider_->GetDeviceKeys(
        CmtgDeviceKeyProvider::Operation::kMakeCredential,
        base::BindOnce([](base::expected<std::vector<std::vector<uint8_t>>,
                                         CmtgDeviceKeyProvider::Error> res) {
          FAIL() << "Callback should have been cancelled.";
        }));
    // Destroy request immediately before task runs.
  }
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("WebAuthentication.CmtgDeviceKeys.Result",
                                    0);
  histogram_tester.ExpectTotalCount(
      "WebAuthentication.CmtgDeviceKeys.RequestDuration", 0);
}

}  // namespace
}  // namespace webauthn
