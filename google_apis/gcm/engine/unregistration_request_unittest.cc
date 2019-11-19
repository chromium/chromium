// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "google_apis/gcm/engine/gcm_request_test_base.h"
#include "google_apis/gcm/engine/gcm_unregistration_request_handler.h"
#include "google_apis/gcm/engine/instance_id_delete_token_request_handler.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

namespace {
const int kMaxRetries = 2;
const uint64_t kAndroidId = 42UL;
const char kLoginHeader[] = "AidLogin";
const char kAppId[] = "TestAppId";
const char kDeletedAppId[] = "deleted=TestAppId";
const char kDeletedToken[] = "token=SomeToken";
const char kProductCategoryForSubtypes[] = "com.chrome.macosx";
const char kRegistrationURL[] = "http://foo.bar/register";
const uint64_t kSecurityToken = 77UL;
const int kGCMVersion = 40;
const char kInstanceId[] = "IID1";
const char kDeveloperId[] = "Project1";
const char kScope[] = "GCM";

}  // namespace

class UnregistrationRequestTest : public GCMRequestTestBase {
 public:
  UnregistrationRequestTest();
  ~UnregistrationRequestTest() override;

  void UnregistrationCallback(UnregistrationRequest::Status status);

  void OnAboutToCompleteFetch() override;

  int max_retry_count() const { return max_retry_count_; }
  void set_max_retry_count(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

 protected:
  int max_retry_count_;
  bool callback_called_;
  UnregistrationRequest::Status status_;
  std::unique_ptr<UnregistrationRequest> request_;
  FakeGCMStatsRecorder recorder_;
};

UnregistrationRequestTest::UnregistrationRequestTest()
    : max_retry_count_(kMaxRetries),
      callback_called_(false),
      status_(UnregistrationRequest::UNREGISTRATION_STATUS_COUNT) {}

UnregistrationRequestTest::~UnregistrationRequestTest() {}

void UnregistrationRequestTest::UnregistrationCallback(
    UnregistrationRequest::Status status) {
  callback_called_ = true;
  status_ = status;
}

void UnregistrationRequestTest::OnAboutToCompleteFetch() {
  status_ = UnregistrationRequest::UNREGISTRATION_STATUS_COUNT;
  callback_called_ = false;
}

class GCMUnregistrationRequestTest : public UnregistrationRequestTest {
 public:
  GCMUnregistrationRequestTest();
  ~GCMUnregistrationRequestTest() override;

  void CreateRequest();
};

GCMUnregistrationRequestTest::GCMUnregistrationRequestTest() {
}

GCMUnregistrationRequestTest::~GCMUnregistrationRequestTest() {
}

void GCMUnregistrationRequestTest::CreateRequest() {
  UnregistrationRequest::RequestInfo request_info(kAndroidId, kSecurityToken,
                                                  kAppId /* category */,
                                                  std::string() /* subtype */);
  std::unique_ptr<GCMUnregistrationRequestHandler> request_handler(
      new GCMUnregistrationRequestHandler(kAppId));
  request_.reset(new UnregistrationRequest(
      GURL(kRegistrationURL), request_info, std::move(request_handler),
      GetBackoffPolicy(),
      base::Bind(&UnregistrationRequestTest::UnregistrationCallback,
                 base::Unretained(this)),
      max_retry_count_, url_loader_factory(),
      base::ThreadTaskRunnerHandle::Get(), &recorder_, std::string()));
}

TEST_F(GCMUnregistrationRequestTest, RequestDataPassedToFetcher) {
  CreateRequest();
  request_->Start();

  // Verify that the no-cookie flag is set.
  const network::ResourceRequest* pending_request;
  ASSERT_TRUE(
      test_url_loader_factory()->IsPending(kRegistrationURL, &pending_request));
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            pending_request->credentials_mode);

  // Verify that authorization header was put together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kRegistrationURL);
  ASSERT_TRUE(headers);
  std::string auth_header;
  headers->GetHeader(net::HttpRequestHeaders::kAuthorization, &auth_header);
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["app"] = kAppId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["delete"] = "true";
  expected_pairs["gcm_unreg_caller"] = "false";

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(GCMUnregistrationRequestTest, SuccessfulUnregistration) {
  set_max_retry_count(0);
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_UNAUTHORIZED, "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, ResponseEmpty) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, InvalidParametersError) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=INVALID_PARAMETERS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::INVALID_PARAMETERS, status_);
}

TEST_F(GCMUnregistrationRequestTest, DeviceRegistrationError) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=PHONE_REGISTRATION_ERROR");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::DEVICE_REGISTRATION_ERROR, status_);
}

TEST_F(GCMUnregistrationRequestTest, UnkwnownError) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "Error=XXX");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::UNKNOWN_ERROR, status_);
}

TEST_F(GCMUnregistrationRequestTest, ServiceUnavailable) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_SERVICE_UNAVAILABLE,
                               "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, InternalServerError) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL,
                               net::HTTP_INTERNAL_SERVER_ERROR, "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, IncorrectAppId) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "deleted=OtherTestAppId");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, ResponseParsingFailed) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "some malformed response");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedAppId);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(GCMUnregistrationRequestTest, MaximumAttemptsReachedWithZeroRetries) {
  set_max_retry_count(0);
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "bad response");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::REACHED_MAX_RETRIES, status_);
}

TEST_F(GCMUnregistrationRequestTest, MaximumAttemptsReached) {
  CreateRequest();
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "bad response");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "bad response");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "bad response");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::REACHED_MAX_RETRIES, status_);
}

class InstaceIDDeleteTokenRequestTest : public UnregistrationRequestTest {
 public:
  InstaceIDDeleteTokenRequestTest();
  ~InstaceIDDeleteTokenRequestTest() override;

  void CreateRequest(bool use_subtype,
                     const std::string& instance_id,
                     const std::string& authorized_entity,
                     const std::string& scope);
};

InstaceIDDeleteTokenRequestTest::InstaceIDDeleteTokenRequestTest() {
}

InstaceIDDeleteTokenRequestTest::~InstaceIDDeleteTokenRequestTest() {
}

void InstaceIDDeleteTokenRequestTest::CreateRequest(
    bool use_subtype,
    const std::string& instance_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  std::string category = use_subtype ? kProductCategoryForSubtypes : kAppId;
  std::string subtype = use_subtype ? kAppId : std::string();
  UnregistrationRequest::RequestInfo request_info(kAndroidId, kSecurityToken,
                                                  category, subtype);
  std::unique_ptr<InstanceIDDeleteTokenRequestHandler> request_handler(
      new InstanceIDDeleteTokenRequestHandler(instance_id, authorized_entity,
                                              scope, kGCMVersion));
  request_.reset(new UnregistrationRequest(
      GURL(kRegistrationURL), request_info, std::move(request_handler),
      GetBackoffPolicy(),
      base::Bind(&UnregistrationRequestTest::UnregistrationCallback,
                 base::Unretained(this)),
      max_retry_count(), url_loader_factory(),
      base::ThreadTaskRunnerHandle::Get(), &recorder_, std::string()));
}

TEST_F(InstaceIDDeleteTokenRequestTest, RequestDataPassedToFetcher) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  // Verify that authorization header was put together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kRegistrationURL);
  ASSERT_TRUE(headers);
  std::string auth_header;
  headers->GetHeader(net::HttpRequestHeaders::kAuthorization, &auth_header);
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["gmsv"] = base::NumberToString(kGCMVersion);
  expected_pairs["app"] = kAppId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["delete"] = "true";
  expected_pairs["appid"] = kInstanceId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["scope"] = kScope;
  expected_pairs["X-scope"] = kScope;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(InstaceIDDeleteTokenRequestTest, RequestDataWithSubtype) {
  CreateRequest(true /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  // Same as RequestDataPassedToFetcher except "app" and "X-subtype".
  std::map<std::string, std::string> expected_pairs;
  expected_pairs["gmsv"] = base::NumberToString(kGCMVersion);
  expected_pairs["app"] = kProductCategoryForSubtypes;
  expected_pairs["X-subtype"] = kAppId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["delete"] = "true";
  expected_pairs["appid"] = kInstanceId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["scope"] = kScope;
  expected_pairs["X-scope"] = kScope;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(InstaceIDDeleteTokenRequestTest, SuccessfulUnregistration) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedToken);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(InstaceIDDeleteTokenRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_UNAUTHORIZED, "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, kDeletedToken);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(InstaceIDDeleteTokenRequestTest, InvalidParametersError) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=INVALID_PARAMETERS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::INVALID_PARAMETERS, status_);
}

TEST_F(InstaceIDDeleteTokenRequestTest, UnkwnownError) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope);
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "Error=XXX");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::UNKNOWN_ERROR, status_);
}

}  // namespace gcm
