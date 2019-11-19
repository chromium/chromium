// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/telemetry_log_writer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "remoting/base/chromoting_event.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kFakeAccessToken[] = "access_token";
constexpr char kAuthorizationHeaderPrefix[] = "Authorization:Bearer ";

class FakeUrlRequest : public UrlRequest {
 public:
  FakeUrlRequest(const std::string& expected_post,
                 const UrlRequest::Result& returned_result)
      : expected_post_(expected_post), returned_result_(returned_result) {}

  void Respond() {
    on_result_callback_.Run(returned_result_);

    // Responding to current request will trigger sending pending events. Call
    // RunUntilIdle() to allow the new request to be created. See LogFakeEvent()
    // below.
    base::RunLoop().RunUntilIdle();
  }

  // UrlRequest overrides.
  void SetPostData(const std::string& content_type,
                   const std::string& post_data) override {
    EXPECT_EQ(content_type, "application/json");
    EXPECT_EQ(post_data, expected_post_);
  }

  void AddHeader(const std::string& value) override {
    if (value.find(kAuthorizationHeaderPrefix) == 0) {
      EXPECT_EQ(std::string(kAuthorizationHeaderPrefix) + kFakeAccessToken,
                value);
    }
  }

  void Start(const OnResultCallback& on_result_callback) override {
    on_result_callback_ = on_result_callback;
  }

 private:
  std::string expected_post_;
  UrlRequest::Result returned_result_;
  OnResultCallback on_result_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeUrlRequest);
};

class FakeUrlRequestFactory : public UrlRequestFactory {
 public:
  ~FakeUrlRequestFactory() override { EXPECT_TRUE(expected_requests_.empty()); }

  // Returns a respond closure. Run this closure to respond to the URL request.
  base::Closure AddExpectedRequest(const std::string& exp_post,
                                   const UrlRequest::Result& ret_result) {
    FakeUrlRequest* fakeRequest = new FakeUrlRequest(exp_post, ret_result);
    base::Closure closure =
        base::Bind(&FakeUrlRequest::Respond, base::Unretained(fakeRequest));
    expected_requests_.push_back(std::unique_ptr<UrlRequest>(fakeRequest));
    return closure;
  }

  // request_factory_ override.
  std::unique_ptr<UrlRequest> CreateUrlRequest(
      UrlRequest::Type type,
      const std::string& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    EXPECT_FALSE(expected_requests_.empty());
    if (expected_requests_.empty()) {
      return std::unique_ptr<UrlRequest>(nullptr);
    }
    EXPECT_EQ(type, UrlRequest::Type::POST);
    std::unique_ptr<UrlRequest> request(std::move(expected_requests_.front()));
    expected_requests_.pop_front();
    return request;
  }

 private:
  base::circular_deque<std::unique_ptr<UrlRequest>> expected_requests_;
};

}  // namespace

class TelemetryLogWriterTest : public testing::Test {
 public:
  TelemetryLogWriterTest()
      : log_writer_(
            "",
            std::make_unique<FakeOAuthTokenGetter>(OAuthTokenGetter::SUCCESS,
                                                   "email",
                                                   kFakeAccessToken)) {
    auto request_factory = std::make_unique<FakeUrlRequestFactory>();
    request_factory_ = request_factory.get();
    log_writer_.Init(std::move(request_factory));
    success_result_.success = true;
    success_result_.status = 200;
    success_result_.response_body = "{}";

    unauth_result_.success = false;
    unauth_result_.status = net::HTTP_UNAUTHORIZED;
    unauth_result_.response_body = "{}";
  }

 protected:
  void LogFakeEvent() {
    ChromotingEvent entry;
    entry.SetInteger("id", id_);
    id_++;
    log_writer_.Log(entry);

    // It's an async process to create request to send all pending events.
    base::RunLoop().RunUntilIdle();
  }

  UrlRequest::Result success_result_;
  UrlRequest::Result unauth_result_;

  FakeUrlRequestFactory* request_factory_;  // No ownership.
  TelemetryLogWriter log_writer_;

 private:
  int id_ = 0;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Test workflow: add request -> log event -> respond request.
// Test fails if req is incorrect or creates more/less reqs than expected
TEST_F(TelemetryLogWriterTest, PostOneLogImmediately) {
  auto respond = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  LogFakeEvent();
  respond.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneLogAndHaveTwoPendingLogs) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":1},{\"id\":2}]}", success_result_);
  LogFakeEvent();
  LogFakeEvent();
  respond1.Run();
  respond2.Run();
}

TEST_F(TelemetryLogWriterTest, PostLogFailedAndRetry) {
  // kMaxTries = 5
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond3 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond4 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond5 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());

  LogFakeEvent();

  respond1.Run();
  respond2.Run();
  respond3.Run();
  respond4.Run();
  respond5.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneLogFailedResendWithTwoPendingLogs) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0},{\"id\":1},{\"id\":2}]}", success_result_);
  LogFakeEvent();
  LogFakeEvent();

  respond1.Run();
  respond2.Run();
}

TEST_F(TelemetryLogWriterTest, PostThreeLogsFailedAndResendWithOnePending) {
  // This tests the ordering of the resent log.
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0},{\"id\":1},{\"id\":2}]}",
      UrlRequest::Result::Failed());
  LogFakeEvent();
  LogFakeEvent();

  respond1.Run();

  auto respond3 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0},{\"id\":1},{\"id\":2},{\"id\":3}]}",
      success_result_);
  LogFakeEvent();

  respond2.Run();
  respond3.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneUnauthorizedCallClosureAndRetry) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", unauth_result_);
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  respond1.Run();
  respond2.Run();
}

}  // namespace remoting
