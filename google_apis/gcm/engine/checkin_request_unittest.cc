// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/gcm_request_test_base.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

const uint64_t kAndroidId = 42UL;
const uint64_t kBlankAndroidId = 999999UL;
const uint64_t kBlankSecurityToken = 999999UL;
const char kCheckinURL[] = "http://foo.bar/checkin";
const char kChromeVersion[] = "Version String";
const uint64_t kSecurityToken = 77;
const char kSettingsDigest[] = "settings_digest";
const char kEmailAddress[] = "test_user@gmail.com";
const char kTokenValue[] = "token_value";

class CheckinRequestTest : public GCMRequestTestBase {
 public:
  enum ResponseScenario {
    VALID_RESPONSE,  // Both android_id and security_token set in response.
    MISSING_ANDROID_ID,  // android_id is missing.
    MISSING_SECURITY_TOKEN,  // security_token is missing.
    ANDROID_ID_IS_ZER0,  // android_id is 0.
    SECURITY_TOKEN_IS_ZERO  // security_token is 0.
  };

  CheckinRequestTest();
  ~CheckinRequestTest() override;

  void FetcherCallback(net::HttpStatusCode response_code,
                       const checkin_proto::AndroidCheckinResponse& response);

  void CreateRequest(uint64_t android_id, uint64_t security_token);

  void SetResponseScenarioAndComplete(ResponseScenario response_scenario);

 protected:
  bool callback_called_;
  net::HttpStatusCode response_code_ =
      net::HTTP_CONTINUE;  // Something that's not used in tests.
  uint64_t android_id_;
  uint64_t security_token_;
  int checkin_device_type_;
  checkin_proto::ChromeBuildProto chrome_build_proto_;
  std::unique_ptr<CheckinRequest> request_;
  FakeGCMStatsRecorder recorder_;
};

CheckinRequestTest::CheckinRequestTest()
    : callback_called_(false),
      android_id_(kBlankAndroidId),
      security_token_(kBlankSecurityToken),
      checkin_device_type_(0) {
}

CheckinRequestTest::~CheckinRequestTest() {}

void CheckinRequestTest::FetcherCallback(
    net::HttpStatusCode response_code,
    const checkin_proto::AndroidCheckinResponse& checkin_response) {
  callback_called_ = true;
  response_code_ = response_code;
  if (checkin_response.has_android_id())
    android_id_ = checkin_response.android_id();
  if (checkin_response.has_security_token())
    security_token_ = checkin_response.security_token();
}

void CheckinRequestTest::CreateRequest(uint64_t android_id,
                                       uint64_t security_token) {
  // First setup a chrome_build protobuf.
  chrome_build_proto_.set_platform(
      checkin_proto::ChromeBuildProto::PLATFORM_LINUX);
  chrome_build_proto_.set_channel(
      checkin_proto::ChromeBuildProto::CHANNEL_CANARY);
  chrome_build_proto_.set_chrome_version(kChromeVersion);

  std::map<std::string, std::string> account_tokens;
  account_tokens[kEmailAddress] = kTokenValue;

  CheckinRequest::RequestInfo request_info(android_id,
                                           security_token,
                                           account_tokens,
                                           kSettingsDigest,
                                           chrome_build_proto_);
  // Then create a request with that protobuf and specified android_id,
  // security_token.
  request_.reset(new CheckinRequest(
      GURL(kCheckinURL), request_info, GetBackoffPolicy(),
      base::Bind(&CheckinRequestTest::FetcherCallback, base::Unretained(this)),
      url_loader_factory(), base::ThreadTaskRunnerHandle::Get(), &recorder_));

  // Setting android_id_ and security_token_ to blank value, not used elsewhere
  // in the tests.
  callback_called_ = false;
  android_id_ = kBlankAndroidId;
  security_token_ = kBlankSecurityToken;
}

void CheckinRequestTest::SetResponseScenarioAndComplete(
    ResponseScenario response_scenario) {
  checkin_proto::AndroidCheckinResponse response;
  response.set_stats_ok(true);

  uint64_t android_id =
      response_scenario == ANDROID_ID_IS_ZER0 ? 0 : kAndroidId;
  uint64_t security_token =
      response_scenario == SECURITY_TOKEN_IS_ZERO ? 0 : kSecurityToken;

  if (response_scenario != MISSING_ANDROID_ID)
    response.set_android_id(android_id);

  if (response_scenario != MISSING_SECURITY_TOKEN)
    response.set_security_token(security_token);

  std::string response_string;
  response.SerializeToString(&response_string);
  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_OK, response_string);
}

TEST_F(CheckinRequestTest, FetcherDataAndURL) {
  CreateRequest(kAndroidId, kSecurityToken);
  request_->Start();

  // Get data sent by request.
  std::string upload_data;
  ASSERT_TRUE(GetUploadDataForURL(kCheckinURL, &upload_data));

  checkin_proto::AndroidCheckinRequest request_proto;
  request_proto.ParseFromString(upload_data);
  EXPECT_EQ(kAndroidId, static_cast<uint64_t>(request_proto.id()));
  EXPECT_EQ(kSecurityToken, request_proto.security_token());
  EXPECT_EQ(chrome_build_proto_.platform(),
            request_proto.checkin().chrome_build().platform());
  EXPECT_EQ(chrome_build_proto_.chrome_version(),
            request_proto.checkin().chrome_build().chrome_version());
  EXPECT_EQ(chrome_build_proto_.channel(),
            request_proto.checkin().chrome_build().channel());
  EXPECT_EQ(2, request_proto.account_cookie_size());
  EXPECT_EQ(kEmailAddress, request_proto.account_cookie(0));
  EXPECT_EQ(kTokenValue, request_proto.account_cookie(1));

#if defined(CHROME_OS)
  EXPECT_EQ(checkin_proto::DEVICE_CHROME_OS, request_proto.checkin().type());
#else
  EXPECT_EQ(checkin_proto::DEVICE_CHROME_BROWSER,
            request_proto.checkin().type());
#endif

  EXPECT_EQ(kSettingsDigest, request_proto.digest());
}

TEST_F(CheckinRequestTest, ResponseBodyEmpty) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_OK, std::string());
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(net::HTTP_OK, response_code_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseBodyCorrupted) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_OK,
                               "Corrupted response body");
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(net::HTTP_OK, response_code_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusUnauthorized) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_UNAUTHORIZED,
                               std::string());
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(net::HTTP_UNAUTHORIZED, response_code_);
  EXPECT_EQ(kBlankAndroidId, android_id_);
  EXPECT_EQ(kBlankSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusBadRequest) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_BAD_REQUEST,
                               std::string());
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(net::HTTP_BAD_REQUEST, response_code_);
  EXPECT_EQ(kBlankAndroidId, android_id_);
  EXPECT_EQ(kBlankSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseForURLAndComplete(kCheckinURL, net::HTTP_INTERNAL_SERVER_ERROR,
                               std::string());
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(net::HTTP_OK, response_code_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseMissingAndroidId) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseScenarioAndComplete(MISSING_ANDROID_ID);
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseMissingSecurityToken) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseScenarioAndComplete(MISSING_SECURITY_TOKEN);
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, AndroidIdEqualsZeroInResponse) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseScenarioAndComplete(ANDROID_ID_IS_ZER0);
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SecurityTokenEqualsZeroInResponse) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseScenarioAndComplete(SECURITY_TOKEN_IS_ZERO);
  EXPECT_FALSE(callback_called_);

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SuccessfulFirstTimeCheckin) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SuccessfulSubsequentCheckin) {
  CreateRequest(kAndroidId, kSecurityToken);
  request_->Start();

  SetResponseScenarioAndComplete(VALID_RESPONSE);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

}  // namespace gcm
