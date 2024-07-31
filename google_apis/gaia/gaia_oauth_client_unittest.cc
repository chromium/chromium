// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthClient.

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pointee;
using ::testing::SaveArg;

namespace {

// Simulates some number of failures, followed by an optional success.
// Does not distinguish between different URLs.
class ResponseInjector {
 public:
  explicit ResponseInjector(network::TestURLLoaderFactory* url_loader_factory)
      : url_loader_factory_(url_loader_factory),
        response_code_(net::HTTP_OK),
        complete_immediately_(true),
        current_failure_count_(0),
        max_failure_count_(0) {
    url_loader_factory->SetInterceptor(
        base::BindRepeating(&ResponseInjector::AdjustResponseBasedOnSettings,
                            base::Unretained(this)));
  }

  ResponseInjector(const ResponseInjector&) = delete;
  ResponseInjector& operator=(const ResponseInjector&) = delete;

  ~ResponseInjector() {
    url_loader_factory_->SetInterceptor(
        base::BindRepeating([](const network::ResourceRequest& request) {
          ADD_FAILURE() << "Unexpected fetch of:" << request.url;
        }));
  }

  void AdjustResponseBasedOnSettings(const network::ResourceRequest& request) {
    url_loader_factory_->ClearResponses();
    DCHECK(pending_url_.is_empty());
    pending_url_ = request.url;
    if (complete_immediately_) {
      Finish();
    }
  }

  void Finish() {
    net::HttpStatusCode response_code = response_code_;
    if (response_code_ != net::HTTP_OK && (max_failure_count_ != -1) &&
        (current_failure_count_ == max_failure_count_))
      response_code = net::HTTP_OK;

    if (response_code != net::HTTP_OK)
      ++current_failure_count_;

    url_loader_factory_->AddResponse(pending_url_.spec(), results_,
                                     response_code);
    pending_url_ = GURL();
  }

  std::string GetUploadData() {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *url_loader_factory_->pending_requests();
    if (pending.size() == 1) {
      return network::GetUploadData(pending[0].request);
    } else {
      ADD_FAILURE() << "Unexpected state in GetUploadData";
      return "";
    }
  }

  const net::HttpRequestHeaders GetRequestHeaders() {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>& pending =
        *url_loader_factory_->pending_requests();
    if (pending.size() == 1) {
      return pending[0].request.headers;
    } else {
      ADD_FAILURE() << "Unexpected state in GetRequestHeaders";
      return {};
    }
  }

  void set_response_code(int response_code) {
    response_code_ = static_cast<net::HttpStatusCode>(response_code);
  }

  void set_max_failure_count(int count) {
    max_failure_count_ = count;
  }

  void set_results(const std::string& results) {
    results_ = results;
  }

  void set_complete_immediately(bool complete_immediately) {
    complete_immediately_ = complete_immediately;
  }
 private:
  raw_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  GURL pending_url_;

  net::HttpStatusCode response_code_;
  bool complete_immediately_;
  int current_failure_count_;
  int max_failure_count_;
  std::string results_;
};

const std::string kTestAccessToken = "1/fFAGRNJru1FTz70BzhT3Zg";
const std::string kTestAccessTokenHandle = "1/kjhH87dfgkj87Hhj5KJkjZ";
const std::string kTestRefreshToken =
    "1/6BMfW9j53gdGImsixUH6kU5RsR4zwI9lUVX-tqf8JXQ";
const std::string kTestUserEmail = "a_user@gmail.com";
const std::string kTestUserId = "8675309";
const int kTestExpiresIn = 3920;

const std::string kDummyGetTokensResult =
    "{\"access_token\":\"" + kTestAccessToken + "\","
    "\"expires_in\":" + base::NumberToString(kTestExpiresIn) + ","
    "\"refresh_token\":\"" + kTestRefreshToken + "\"}";

const std::string kDummyRefreshTokenResult =
    "{\"access_token\":\"" + kTestAccessToken + "\","
    "\"expires_in\":" + base::NumberToString(kTestExpiresIn) + "}";

const std::string kDummyUserInfoResult =
    "{\"email\":\"" + kTestUserEmail + "\"}";

const std::string kDummyUserIdResult =
    "{\"id\":\"" + kTestUserId + "\"}";

const std::string kDummyFullUserInfoResult =
    "{"
      "\"family_name\": \"Bar\", "
      "\"name\": \"Foo Bar\", "
      "\"picture\": \"https://lh4.googleusercontent.com/hash/photo.jpg\", "
      "\"locale\": \"en\", "
      "\"gender\": \"male\", "
      "\"link\": \"https://plus.google.com/+FooBar\", "
      "\"given_name\": \"Foo\", "
      "\"id\": \"12345678901234567890\""
    "}";

const std::string kDummyTokenInfoResult =
    "{\"issued_to\": \"1234567890.apps.googleusercontent.com\","
    "\"audience\": \"1234567890.apps.googleusercontent.com\","
    "\"scope\": \"https://googleapis.com/oauth2/v2/tokeninfo\","
    "\"expires_in\":" + base::NumberToString(kTestExpiresIn) + "}";

const std::string kDummyTokenHandleInfoResult =
    "{\"audience\": \"1234567890.apps.googleusercontent.com\","
    "\"expires_in\":" + base::NumberToString(kTestExpiresIn) + "}";

const std::string kDummyAccountCapabilitiesResult =
    "{\"accountCapabilities\": ["
    "{\"name\": \"accountcapabilities/111\", \"booleanValue\": false},"
    "{\"name\": \"accountcapabilities/222\", \"booleanValue\": true}"
    "]}";

}  // namespace

namespace gaia {

class MockGaiaOAuthClientDelegate : public gaia::GaiaOAuthClient::Delegate {
 public:
  MockGaiaOAuthClientDelegate() = default;

  MockGaiaOAuthClientDelegate(const MockGaiaOAuthClientDelegate&) = delete;
  MockGaiaOAuthClientDelegate& operator=(const MockGaiaOAuthClientDelegate&) =
      delete;

  MOCK_METHOD3(OnGetTokensResponse,
               void(const std::string& refresh_token,
                    const std::string& access_token,
                    int expires_in_seconds));
  MOCK_METHOD2(OnRefreshTokenResponse,
               void(const std::string& access_token, int expires_in_seconds));
  MOCK_METHOD1(OnGetUserEmailResponse, void(const std::string& user_email));
  MOCK_METHOD1(OnGetUserIdResponse, void(const std::string& user_id));
  MOCK_METHOD1(OnGetUserInfoResponse, void(const base::Value::Dict& user_info));
  MOCK_METHOD1(OnGetTokenInfoResponse,
               void(const base::Value::Dict& token_info));
  MOCK_METHOD1(OnGetAccountCapabilitiesResponse,
               void(const base::Value::Dict& account_capabilities));
  MOCK_METHOD0(OnOAuthError, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));
};

class GaiaOAuthClientTest : public testing::Test {
 protected:
  GaiaOAuthClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    client_info_.client_id = "test_client_id";
    client_info_.client_secret = "test_client_secret";
    client_info_.redirect_uri = "test_redirect_uri";
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &url_loader_factory_);
  }

  void FlushNetwork() {
    // An event loop spin is required for things to be delivered from
    // TestURLLoaderFactory to its clients via mojo pipes. In addition,
    // some retries may have back off, so may need to advance (mock) time
    // for them to finish, too.
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void TestAccountCapabilitiesUploadData(
      const std::vector<std::string>& capabilities_names,
      const std::string& expected_body) {
    ResponseInjector injector(&url_loader_factory_);
    injector.set_complete_immediately(false);

    MockGaiaOAuthClientDelegate delegate;
    GaiaOAuthClient auth(GetSharedURLLoaderFactory());
    auth.GetAccountCapabilities("some_token", capabilities_names, 1, &delegate);

    EXPECT_EQ(injector.GetUploadData(), expected_body);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;

  OAuthClientInfo client_info_;
};

TEST_F(GaiaOAuthClientTest, NetworkFailure) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnNetworkError(response_code))
      .Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_response_code(response_code);
  injector.set_max_failure_count(4);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", 2, &delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, NetworkFailureRecover) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_response_code(response_code);
  injector.set_max_failure_count(4);
  injector.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, NetworkFailureRecoverBackoff) {
  // Make sure long backoffs are expontential.
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
                                            kTestExpiresIn))
      .Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_response_code(response_code);
  injector.set_max_failure_count(21);
  injector.set_results(kDummyGetTokensResult);

  base::TimeTicks start = task_environment_.GetMockTickClock()->NowTicks();

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
  FlushNetwork();

  // Default params are:
  //    40% jitter, 700ms initial, 1.4 exponent, ignore first 2 failures.
  // So after 19 retries, delay is at least:
  //    0.6 * 700ms * 1.4^(19-2) ~ 128s
  // After 20:
  //    0.6 * 700ms * 1.4^(20-2) ~ 179s
  //
  // ... so the whole thing should take at least 307s
  EXPECT_GE(task_environment_.GetMockTickClock()->NowTicks() - start,
            base::Seconds(307));
}

TEST_F(GaiaOAuthClientTest, OAuthFailure) {
  int response_code = net::HTTP_BAD_REQUEST;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnOAuthError()).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_response_code(response_code);
  injector.set_max_failure_count(-1);
  injector.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
  FlushNetwork();
}


TEST_F(GaiaOAuthClientTest, GetTokensSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokensResponse(kTestRefreshToken, kTestAccessToken,
      kTestExpiresIn)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, GetTokensAfterNetworkFailure) {
  int response_code = net::HTTP_INTERNAL_SERVER_ERROR;

  MockGaiaOAuthClientDelegate failure_delegate;
  EXPECT_CALL(failure_delegate, OnNetworkError(response_code)).Times(1);

  MockGaiaOAuthClientDelegate success_delegate;
  EXPECT_CALL(success_delegate, OnGetTokensResponse(kTestRefreshToken,
      kTestAccessToken, kTestExpiresIn)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_response_code(response_code);
  injector.set_max_failure_count(4);
  injector.set_results(kDummyGetTokensResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokensFromAuthCode(client_info_, "auth_code", 2, &failure_delegate);
  FlushNetwork();
  auth.GetTokensFromAuthCode(client_info_, "auth_code", -1, &success_delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, RefreshTokenSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyRefreshTokenResult);
  injector.set_complete_immediately(false);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.RefreshToken(client_info_, "refresh_token", std::vector<std::string>(),
                    -1, &delegate);
  EXPECT_THAT(injector.GetUploadData(), Not(HasSubstr("scope")));
  injector.Finish();
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, RefreshTokenDownscopingSuccess) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnRefreshTokenResponse(kTestAccessToken,
      kTestExpiresIn)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyRefreshTokenResult);
  injector.set_complete_immediately(false);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.RefreshToken(client_info_, "refresh_token",
                    std::vector<std::string>(1, "scope4test"), -1, &delegate);
  EXPECT_THAT(injector.GetUploadData(), HasSubstr("&scope=scope4test"));
  injector.Finish();
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, GetUserEmail) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetUserEmailResponse(kTestUserEmail)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyUserInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetUserEmail("access_token", 1, &delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, GetUserId) {
  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetUserIdResponse(kTestUserId)).Times(1);

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyUserIdResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetUserId("access_token", 1, &delegate);
  FlushNetwork();
}

TEST_F(GaiaOAuthClientTest, GetUserInfo) {
  base::Value::Dict captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetUserInfoResponse(_))
      .WillOnce([&](const base::Value::Dict& result) {
        captured_result = result.Clone();
      });

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyFullUserInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetUserInfo("access_token", 1, &delegate);
  FlushNetwork();

  std::optional<base::Value> expected_value =
      base::JSONReader::Read(kDummyFullUserInfoResult);
  DCHECK(expected_value);
  ASSERT_TRUE(expected_value->is_dict());
  EXPECT_EQ(expected_value->GetDict(), captured_result);
}

TEST_F(GaiaOAuthClientTest, GetTokenInfo) {
  base::Value::Dict captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokenInfoResponse(_))
      .WillOnce([&](const base::Value::Dict& result) {
        captured_result = result.Clone();
      });

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyTokenInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokenInfo("some_token", 1, &delegate);
  FlushNetwork();

  std::string* issued_to = captured_result.FindString("issued_to");
  ASSERT_NE(issued_to, nullptr);
  ASSERT_EQ("1234567890.apps.googleusercontent.com", *issued_to);
}

TEST_F(GaiaOAuthClientTest, GetTokenHandleInfo) {
  base::Value::Dict captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokenInfoResponse(_))
      .WillOnce([&](const base::Value::Dict& result) {
        captured_result = result.Clone();
      });

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyTokenHandleInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokenHandleInfo("some_handle", 1, &delegate);
  FlushNetwork();

  std::string* audience = captured_result.FindString("audience");
  ASSERT_NE(audience, nullptr);
  ASSERT_EQ("1234567890.apps.googleusercontent.com", *audience);
}

TEST_F(GaiaOAuthClientTest, GetAccountCapabilities) {
  base::Value::Dict captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetAccountCapabilitiesResponse(_))
      .WillOnce([&](const base::Value::Dict& result) {
        captured_result = result.Clone();
      });

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyAccountCapabilitiesResult);
  injector.set_complete_immediately(false);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetAccountCapabilities("some_token",
                              {"capability1", "capability2", "capability3"}, 1,
                              &delegate);

  EXPECT_THAT(injector.GetRequestHeaders().GetHeader("Authorization"),
              testing::Optional(std::string("Bearer some_token")));
  EXPECT_THAT(injector.GetRequestHeaders().GetHeader("X-HTTP-Method-Override"),
              testing::Optional(std::string("GET")));
  EXPECT_EQ(injector.GetUploadData(),
            "names=capability1&names=capability2&names=capability3");

  injector.Finish();
  FlushNetwork();

  const base::Value::List& capabilities =
      *captured_result.FindList("accountCapabilities");
  ASSERT_EQ(capabilities.size(), 2U);
  EXPECT_EQ(*capabilities[0].GetDict().FindString("name"),
            "accountcapabilities/111");
  EXPECT_FALSE(*capabilities[0].GetDict().FindBool("booleanValue"));
  EXPECT_EQ(*capabilities[1].GetDict().FindString("name"),
            "accountcapabilities/222");
  EXPECT_TRUE(*capabilities[1].GetDict().FindBool("booleanValue"));
}

TEST_F(GaiaOAuthClientTest,
       GetAccountCapabilities_UploadData_OneCapabilityName) {
  TestAccountCapabilitiesUploadData({"capability"},
                                    /*expected_body=*/"names=capability");
}

TEST_F(GaiaOAuthClientTest,
       GetAccountCapabilities_UploadData_MultipleCapabilityNames) {
  TestAccountCapabilitiesUploadData(
      {"capability1", "capability2", "capability3"},
      /*expected_body=*/
      "names=capability1&names=capability2&names=capability3");
}

}  // namespace gaia
