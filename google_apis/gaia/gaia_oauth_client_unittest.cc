// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthClient.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/base/net_errors.h"
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
  network::TestURLLoaderFactory* url_loader_factory_;
  GURL pending_url_;

  net::HttpStatusCode response_code_;
  bool complete_immediately_;
  int current_failure_count_;
  int max_failure_count_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(ResponseInjector);
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

}  // namespace

namespace gaia {

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
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;

  OAuthClientInfo client_info_;
};

class MockGaiaOAuthClientDelegate : public gaia::GaiaOAuthClient::Delegate {
 public:
  MockGaiaOAuthClientDelegate() {}
  ~MockGaiaOAuthClientDelegate() override {}

  MOCK_METHOD3(OnGetTokensResponse, void(const std::string& refresh_token,
                                         const std::string& access_token,
                                         int expires_in_seconds));
  MOCK_METHOD2(OnRefreshTokenResponse, void(const std::string& access_token,
                                            int expires_in_seconds));
  MOCK_METHOD1(OnGetUserEmailResponse, void(const std::string& user_email));
  MOCK_METHOD1(OnGetUserIdResponse, void(const std::string& user_id));
  MOCK_METHOD0(OnOAuthError, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));

  // gMock doesn't like methods that take or return scoped_ptr.  A
  // work-around is to create a mock method that takes a raw ptr, and
  // override the problematic method to call through to it.
  // https://groups.google.com/a/chromium.org/d/msg/chromium-dev/01sDxsJ1OYw/I_S0xCBRF2oJ
  MOCK_METHOD1(OnGetUserInfoResponsePtr,
               void(const base::DictionaryValue* user_info));
  void OnGetUserInfoResponse(
      std::unique_ptr<base::DictionaryValue> user_info) override {
    user_info_.reset(user_info.release());
    OnGetUserInfoResponsePtr(user_info_.get());
  }
  MOCK_METHOD1(OnGetTokenInfoResponsePtr,
               void(const base::DictionaryValue* token_info));
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override {
    token_info_.reset(token_info.release());
    OnGetTokenInfoResponsePtr(token_info_.get());
  }

 private:
  std::unique_ptr<base::DictionaryValue> user_info_;
  std::unique_ptr<base::DictionaryValue> token_info_;
  DISALLOW_COPY_AND_ASSIGN(MockGaiaOAuthClientDelegate);
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
            base::TimeDelta::FromSeconds(307));
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
  const base::DictionaryValue* captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetUserInfoResponsePtr(_))
      .WillOnce(SaveArg<0>(&captured_result));

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyFullUserInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetUserInfo("access_token", 1, &delegate);
  FlushNetwork();

  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadDeprecated(kDummyFullUserInfoResult);
  DCHECK(value);
  ASSERT_TRUE(value->is_dict());
  base::DictionaryValue* expected_result;
  value->GetAsDictionary(&expected_result);

  ASSERT_TRUE(expected_result->Equals(captured_result));
}

TEST_F(GaiaOAuthClientTest, GetTokenInfo) {
  const base::DictionaryValue* captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokenInfoResponsePtr(_))
      .WillOnce(SaveArg<0>(&captured_result));

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyTokenInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokenInfo("some_token", 1, &delegate);
  FlushNetwork();

  std::string issued_to;
  ASSERT_TRUE(captured_result->GetString("issued_to", &issued_to));
  ASSERT_EQ("1234567890.apps.googleusercontent.com", issued_to);
}

TEST_F(GaiaOAuthClientTest, GetTokenHandleInfo) {
  const base::DictionaryValue* captured_result;

  MockGaiaOAuthClientDelegate delegate;
  EXPECT_CALL(delegate, OnGetTokenInfoResponsePtr(_))
      .WillOnce(SaveArg<0>(&captured_result));

  ResponseInjector injector(&url_loader_factory_);
  injector.set_results(kDummyTokenHandleInfoResult);

  GaiaOAuthClient auth(GetSharedURLLoaderFactory());
  auth.GetTokenHandleInfo("some_handle", 1, &delegate);
  FlushNetwork();

  std::string audience;
  ASSERT_TRUE(captured_result->GetString("audience", &audience));
  ASSERT_EQ("1234567890.apps.googleusercontent.com", audience);
}

}  // namespace gaia
