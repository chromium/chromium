// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/single_thread_task_runner.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gcm/engine/gcm_registration_request_handler.h"
#include "google_apis/gcm/engine/gcm_request_test_base.h"
#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

namespace {
const uint64_t kAndroidId = 42UL;
const char kAppId[] = "TestAppId";
const char kProductCategoryForSubtypes[] = "com.chrome.macosx";
const char kDeveloperId[] = "Project1";
const char kLoginHeader[] = "AidLogin";
const char kRegistrationURL[] = "http://foo.bar/register";
const uint64_t kSecurityToken = 77UL;
const int kGCMVersion = 40;
const char kInstanceId[] = "IID1";
const char kScope[] = "GCM";

}  // namespace

class RegistrationRequestTest : public GCMRequestTestBase {
 public:
  RegistrationRequestTest();
  ~RegistrationRequestTest() override;

  void RegistrationCallback(RegistrationRequest::Status status,
                            const std::string& registration_id);

  void OnAboutToCompleteFetch() override;

  void set_max_retry_count(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

 protected:
  int max_retry_count_;
  RegistrationRequest::Status status_;
  std::string registration_id_;
  bool callback_called_;
  std::unique_ptr<RegistrationRequest> request_;
  FakeGCMStatsRecorder recorder_;
};

RegistrationRequestTest::RegistrationRequestTest()
    : max_retry_count_(2),
      status_(RegistrationRequest::SUCCESS),
      callback_called_(false) {}

RegistrationRequestTest::~RegistrationRequestTest() {}

void RegistrationRequestTest::RegistrationCallback(
    RegistrationRequest::Status status,
    const std::string& registration_id) {
  status_ = status;
  registration_id_ = registration_id;
  callback_called_ = true;
}

void RegistrationRequestTest::OnAboutToCompleteFetch() {
  registration_id_.clear();
  status_ = RegistrationRequest::SUCCESS;
  callback_called_ = false;
}

class GCMRegistrationRequestTest : public RegistrationRequestTest {
 public:
  GCMRegistrationRequestTest();
  ~GCMRegistrationRequestTest() override;

  void CreateRequest(const std::string& sender_ids);
};

GCMRegistrationRequestTest::GCMRegistrationRequestTest() {
}

GCMRegistrationRequestTest::~GCMRegistrationRequestTest() {
}

void GCMRegistrationRequestTest::CreateRequest(const std::string& sender_ids) {
  RegistrationRequest::RequestInfo request_info(kAndroidId, kSecurityToken,
                                                kAppId /* category */,
                                                std::string() /* subtype */);
  std::unique_ptr<GCMRegistrationRequestHandler> request_handler(
      new GCMRegistrationRequestHandler(sender_ids));
  request_ = std::make_unique<RegistrationRequest>(
      GURL(kRegistrationURL), request_info, std::move(request_handler),
      GetBackoffPolicy(),
      base::BindOnce(&RegistrationRequestTest::RegistrationCallback,
                     base::Unretained(this)),
      max_retry_count_, url_loader_factory(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_,
      sender_ids);
}

TEST_F(GCMRegistrationRequestTest, RequestSuccessful) {
  set_max_retry_count(0);
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, RequestDataAndURL) {
  CreateRequest(kDeveloperId);
  request_->Start();

  // Get data sent by request and verify that authorization header was put
  // together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kRegistrationURL);
  ASSERT_TRUE(headers != nullptr);
  std::optional<std::string> auth_header =
      headers->GetHeader(net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header);
  base::StringTokenizer auth_tokenizer(auth_header.value(), " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(GCMRegistrationRequestTest, RequestRegistrationWithMultipleSenderIds) {
  CreateRequest("sender1,sender2@gmail.com");
  request_->Start();


  // Verify data was formatted properly.
  std::string upload_data;
  ASSERT_TRUE(GetUploadDataForURL(kRegistrationURL, &upload_data));
  base::StringTokenizer data_tokenizer(upload_data, "&=");

  // Skip all tokens until you hit entry for senders.
  while (data_tokenizer.GetNext() && data_tokenizer.token() != "sender")
    continue;

  ASSERT_TRUE(data_tokenizer.GetNext());
  std::string senders(base::UnescapeBinaryURLComponent(data_tokenizer.token()));
  base::StringTokenizer sender_tokenizer(senders, ",");
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender1", sender_tokenizer.token());
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender2@gmail.com", sender_tokenizer.token());
}

TEST_F(GCMRegistrationRequestTest, ResponseParsing) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseParsingFailed) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "tok");  // Simulate truncated message.
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_UNAUTHORIZED,
                               "token=2501");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseMissingRegistrationId) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "some error in response");
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseDeviceRegistrationError) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=PHONE_REGISTRATION_ERROR");
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseAuthenticationError) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_UNAUTHORIZED,
                               "Error=AUTHENTICATION_FAILED");
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseInternalServerError) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               "Error=InternalServerError");
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseInvalidParameters) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=INVALID_PARAMETERS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_PARAMETERS, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseInvalidSender) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=INVALID_SENDER");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_SENDER, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseInvalidSenderBadRequest) {
  CreateRequest("sender1");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_BAD_REQUEST,
                               "Error=INVALID_SENDER");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_SENDER, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseQuotaExceeded) {
  CreateRequest("sender1");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_SERVICE_UNAVAILABLE,
                               "Error=QUOTA_EXCEEDED");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::QUOTA_EXCEEDED, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseTooManyRegistrations) {
  CreateRequest("sender1");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK,
                               "Error=TOO_MANY_REGISTRATIONS");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::TOO_MANY_REGISTRATIONS, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, RequestNotSuccessful) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501",
                               net::ERR_FAILED);
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeded.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, ResponseHttpNotOk) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "token=2501");
  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeded.
  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(GCMRegistrationRequestTest, MaximumAttemptsReachedWithZeroRetries) {
  set_max_retry_count(0);
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "token=2501");

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::REACHED_MAX_RETRIES, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(GCMRegistrationRequestTest, MaximumAttemptsReached) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "token=2501");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "token=2501");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_GATEWAY_TIMEOUT,
                               "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::REACHED_MAX_RETRIES, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

class InstanceIDGetTokenRequestTest : public RegistrationRequestTest {
 public:
  InstanceIDGetTokenRequestTest();
  ~InstanceIDGetTokenRequestTest() override;

  void CreateRequest(bool use_subtype,
                     const std::string& instance_id,
                     const std::string& authorized_entity,
                     const std::string& scope,
                     base::TimeDelta time_to_live);
};

InstanceIDGetTokenRequestTest::InstanceIDGetTokenRequestTest() {
}

InstanceIDGetTokenRequestTest::~InstanceIDGetTokenRequestTest() {
}

void InstanceIDGetTokenRequestTest::CreateRequest(
    bool use_subtype,
    const std::string& instance_id,
    const std::string& authorized_entity,
    const std::string& scope,
    base::TimeDelta time_to_live) {
  std::string category = use_subtype ? kProductCategoryForSubtypes : kAppId;
  std::string subtype = use_subtype ? kAppId : std::string();
  RegistrationRequest::RequestInfo request_info(kAndroidId, kSecurityToken,
                                                category, subtype);
  std::unique_ptr<InstanceIDGetTokenRequestHandler> request_handler(
      new InstanceIDGetTokenRequestHandler(instance_id, authorized_entity,
                                           scope, kGCMVersion, time_to_live));
  request_ = std::make_unique<RegistrationRequest>(
      GURL(kRegistrationURL), request_info, std::move(request_handler),
      GetBackoffPolicy(),
      base::BindOnce(&RegistrationRequestTest::RegistrationCallback,
                     base::Unretained(this)),
      max_retry_count_, url_loader_factory(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_,
      authorized_entity);
}

TEST_F(InstanceIDGetTokenRequestTest, RequestSuccessful) {
  set_max_retry_count(0);
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope,
                /*time_to_live=*/base::TimeDelta());
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(InstanceIDGetTokenRequestTest, RequestDataAndURL) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope,
                /*time_to_live=*/base::TimeDelta());
  request_->Start();

  // Verify that the no-cookie flag is set.
  const network::ResourceRequest* pending_request;
  ASSERT_TRUE(
      test_url_loader_factory()->IsPending(kRegistrationURL, &pending_request));
  EXPECT_EQ(google_apis::GetOmitCredentialsModeForGaiaRequests(),
            pending_request->credentials_mode);

  // Verify that authorization header was put together properly.
  const net::HttpRequestHeaders* headers =
      GetExtraHeadersForURL(kRegistrationURL);
  ASSERT_TRUE(headers != nullptr);
  std::optional<std::string> auth_header =
      headers->GetHeader(net::HttpRequestHeaders::kAuthorization);
  ASSERT_TRUE(auth_header);
  base::StringTokenizer auth_tokenizer(auth_header.value(), " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::NumberToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["gmsv"] = base::NumberToString(kGCMVersion);
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["appid"] = kInstanceId;
  expected_pairs["scope"] = kScope;
  expected_pairs["X-scope"] = kScope;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(InstanceIDGetTokenRequestTest, RequestDataWithTTL) {
  CreateRequest(false, kInstanceId, kDeveloperId, kScope,
                /*time_to_live=*/base::Seconds(100));
  request_->Start();

  // Same as RequestDataAndURL except "ttl" and "X-Foo".
  std::map<std::string, std::string> expected_pairs;
  expected_pairs["gmsv"] = base::NumberToString(kGCMVersion);
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["appid"] = kInstanceId;
  expected_pairs["scope"] = kScope;
  expected_pairs["ttl"] = "100";
  expected_pairs["X-scope"] = kScope;

  ASSERT_NO_FATAL_FAILURE(
      VerifyFetcherUploadDataForURL(kRegistrationURL, &expected_pairs));
}

TEST_F(InstanceIDGetTokenRequestTest, RequestDataWithSubtype) {
  CreateRequest(true /* use_subtype */, kInstanceId, kDeveloperId, kScope,
                /*time_to_live=*/base::TimeDelta());
  request_->Start();

  // Same as RequestDataAndURL except "app" and "X-subtype".
  std::map<std::string, std::string> expected_pairs;
  expected_pairs["gmsv"] = base::NumberToString(kGCMVersion);
  expected_pairs["app"] = kProductCategoryForSubtypes;
  expected_pairs["X-subtype"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["device"] = base::NumberToString(kAndroidId);
  expected_pairs["appid"] = kInstanceId;
  expected_pairs["scope"] = kScope;
  expected_pairs["X-scope"] = kScope;

  // Verify data was formatted properly.
  std::string upload_data;
  ASSERT_TRUE(GetUploadDataForURL(kRegistrationURL, &upload_data));
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    auto iter = expected_pairs.find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs.end());
    ASSERT_TRUE(data_tokenizer.GetNext());
    EXPECT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs.erase(iter);
  }

  EXPECT_EQ(0UL, expected_pairs.size());
}

TEST_F(InstanceIDGetTokenRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest(false /* use_subtype */, kInstanceId, kDeveloperId, kScope,
                /*time_to_live=*/base::TimeDelta());
  request_->Start();

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_UNAUTHORIZED,
                               "token=2501");
  EXPECT_FALSE(callback_called_);

  SetResponseForURLAndComplete(kRegistrationURL, net::HTTP_OK, "token=2501");
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

}  // namespace gcm
