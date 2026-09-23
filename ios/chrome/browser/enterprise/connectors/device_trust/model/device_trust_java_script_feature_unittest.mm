// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_java_script_feature.h"

#import <memory>
#import <optional>
#import <set>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/mock_device_trust_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

constexpr char kChallenge[] = "my_challenge";
constexpr char kAllowedUrl[] = "https://example.com/login";

std::unique_ptr<base::Value> MakeMessageBody(base::Value challenge) {
  base::DictValue body;
  body.Set("challengeRequest", std::move(challenge));
  return std::make_unique<base::Value>(std::move(body));
}

// Subclass to access protected constructor and message handler in tests.
class TestDeviceTrustJavaScriptFeature : public DeviceTrustJavaScriptFeature {
 public:
  using DeviceTrustJavaScriptFeature::DeviceTrustJavaScriptFeature;
  using DeviceTrustJavaScriptFeature::ScriptMessageReceivedWithReply;
};

// Test fixture verifying script message validation and routing.
class DeviceTrustJavaScriptFeatureTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        DeviceTrustServiceFactoryIOS::GetInstance(),
        base::BindOnce([](ProfileIOS*) -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<
              enterprise_connectors::test::MockDeviceTrustService>>();
        }));
    profile_ = std::move(builder).Build();
    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {DeviceTrustJavaScriptFeature::GetInstance()});

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    web_state_->SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
  }

  web::WebState* web_state() { return web_state_.get(); }

  enterprise_connectors::test::MockDeviceTrustService* mock_service() {
    return static_cast<enterprise_connectors::test::MockDeviceTrustService*>(
        DeviceTrustServiceFactoryIOS::GetForProfile(profile_.get()));
  }

  base::OnceCallback<void(const base::Value*, NSString*)> CaptureReplyCallback(
      base::OnceClosure quit_closure = {}) {
    // The test feature invokes reply callbacks on the calling sequence, so this
    // callback cannot outlive the test fixture.
    return base::BindOnce(
        [](DeviceTrustJavaScriptFeatureTest* test,
           base::OnceClosure quit_closure, const base::Value* reply,
           NSString* error) {
          test->reply_received_ = true;
          if (reply) {
            test->reply_ = reply->Clone();
          }
          test->reply_error_ = error;
          if (quit_closure) {
            std::move(quit_closure).Run();
          }
        },
        base::Unretained(this), std::move(quit_closure));
  }

  void SendMessage(std::unique_ptr<base::Value> body,
                   bool is_main_frame = true) {
    web::ScriptMessage message(std::move(body),
                               /*is_user_interacting=*/false, is_main_frame,
                               /*request_url=*/std::nullopt,
                               /*security_origin=*/url::Origin());
    feature_.ScriptMessageReceivedWithReply(web_state(), message,
                                            CaptureReplyCallback());
  }

  void SendChallenge(base::Value challenge) {
    SendMessage(MakeMessageBody(std::move(challenge)));
  }

  void SendChallengeFrom(web::WebState* web_state,
                         base::OnceClosure quit_closure = {}) {
    web::ScriptMessage message(
        MakeMessageBody(base::Value(kChallenge)),
        /*is_user_interacting=*/false,
        /*is_main_frame=*/true,
        /*request_url=*/GURL(kAllowedUrl),
        /*security_origin=*/url::Origin::Create(GURL(kAllowedUrl)));
    feature_.ScriptMessageReceivedWithReply(
        web_state, message, CaptureReplyCallback(std::move(quit_closure)));
  }

  void ResetReply() {
    reply_received_ = false;
    reply_.reset();
    reply_error_ = nil;
  }

  void ExpectErrorReply(std::string_view error_code) {
    ASSERT_TRUE(reply_received_);
    ASSERT_TRUE(reply_.has_value());
    ASSERT_TRUE(reply_->is_dict());
    const std::string* actual_error_code =
        reply_->GetDict().FindString("errorCode");
    ASSERT_NE(actual_error_code, nullptr);
    EXPECT_EQ(*actual_error_code, error_code);
    EXPECT_EQ(reply_error_, nil);
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  TestDeviceTrustJavaScriptFeature feature_;
  bool reply_received_ = false;
  std::optional<base::Value> reply_;
  NSString* reply_error_ = nil;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
};

// Verifies that messages originating from non-main frames are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectNonMainFrameMessage) {
  SendMessage(MakeMessageBody(base::Value(kChallenge)),
              /*is_main_frame=*/false);

  ExpectErrorReply("UNSUPPORTED_FRAME");
  ASSERT_TRUE(reply_.has_value());
  ASSERT_TRUE(reply_->is_dict());
  EXPECT_THAT(reply_->GetDict().FindString("errorMessage"),
              testing::Pointee(testing::StrEq(
                  "Device attestation is only supported in the main frame.")));
}

// Verifies that messages with null bodies are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectMissingBody) {
  SendMessage(nullptr);

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that non-dictionary message bodies are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectNonDictionaryBody) {
  SendMessage(std::make_unique<base::Value>("invalid"));

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that messages missing the 'challengeRequest' key are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectMissingChallenge) {
  SendMessage(std::make_unique<base::Value>(base::DictValue()));

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that challenges of non-string types (e.g. integers) are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectWrongChallengeType) {
  SendChallenge(base::Value(42));

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that empty challenge strings are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectEmptyChallenge) {
  SendChallenge(base::Value(""));

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that challenges exceeding the maximum allowed size are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectOversizedChallenge) {
  SendChallenge(base::Value(std::string(1025, 'a')));

  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that the default production routing returns SERVICE_UNAVAILABLE when
// no DeviceTrustChallengeTabHelper is attached to the WebState.
TEST_F(DeviceTrustJavaScriptFeatureTest,
       DefaultRoutingReturnsServiceUnavailableWhenNoTabHelper) {
  ResetReply();
  web::ScriptMessage message(MakeMessageBody(base::Value(kChallenge)),
                             /*is_user_interacting=*/false,
                             /*is_main_frame=*/true,
                             /*request_url=*/std::nullopt,
                             /*security_origin=*/url::Origin());
  feature_.ScriptMessageReceivedWithReply(web_state(), message,
                                          CaptureReplyCallback());

  ExpectErrorReply("SERVICE_UNAVAILABLE");
  ASSERT_TRUE(reply_.has_value());
  EXPECT_EQ(*reply_->GetDict().FindString("errorMessage"),
            "Device attestation is not available.");
}

// Verifies that the default production routing returns SERVICE_UNAVAILABLE when
// the DeviceTrustService is disabled.
TEST_F(DeviceTrustJavaScriptFeatureTest,
       DefaultRoutingReturnsServiceUnavailableWhenServiceDisabled) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(false));

  ResetReply();
  base::RunLoop run_loop;
  web::ScriptMessage message(
      MakeMessageBody(base::Value(kChallenge)),
      /*is_user_interacting=*/false,
      /*is_main_frame=*/true,
      /*request_url=*/std::nullopt,
      /*security_origin=*/url::Origin::Create(GURL("https://example.com")));
  feature_.ScriptMessageReceivedWithReply(
      web_state(), message, CaptureReplyCallback(run_loop.QuitClosure()));

  run_loop.Run();
  ExpectErrorReply("SERVICE_UNAVAILABLE");
  ASSERT_TRUE(reply_.has_value());
  EXPECT_EQ(*reply_->GetDict().FindString("errorMessage"),
            "Device attestation is not available.");

  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

// Verifies that the default production routing returns URL_NOT_ALLOWED when the
// URL is not in the Device Trust allowlist.
TEST_F(DeviceTrustJavaScriptFeatureTest, DefaultRoutingReturnsUrlNotAllowed) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  EXPECT_CALL(*mock_service(), Watches(GURL("https://example.com")))
      .WillOnce(
          testing::Return(std::set<enterprise_connectors::DTCPolicyLevel>()));

  ResetReply();
  base::RunLoop run_loop;
  web::ScriptMessage message(
      MakeMessageBody(base::Value(kChallenge)),
      /*is_user_interacting=*/false,
      /*is_main_frame=*/true,
      /*request_url=*/std::nullopt,
      /*security_origin=*/url::Origin::Create(GURL("https://example.com")));
  feature_.ScriptMessageReceivedWithReply(
      web_state(), message, CaptureReplyCallback(run_loop.QuitClosure()));

  run_loop.Run();
  ExpectErrorReply("URL_NOT_ALLOWED");
  ASSERT_TRUE(reply_.has_value());
  EXPECT_EQ(*reply_->GetDict().FindString("errorMessage"),
            "The requesting URL is not allowed for device attestation.");

  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

// Verifies that the feature enforces kMaxPendingRequests when processing
// messages via ScriptMessageReceivedWithReply. The first kMaxPendingRequests
// messages are routed to the service, while the subsequent request is rejected
// with TOO_MANY_REQUESTS before reaching the service.
TEST_F(DeviceTrustJavaScriptFeatureTest, PendingRequestsLimitEnforced) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));

  std::vector<enterprise_connectors::DeviceTrustService::DeviceTrustCallback>
      held_callbacks;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .Times(DeviceTrustChallengeTabHelper::kMaxPendingRequests)
      .WillRepeatedly(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { held_callbacks.push_back(std::move(callback)); });

  // Send kMaxPendingRequests valid messages via ScriptMessageReceivedWithReply.
  // All should reach the service and be held pending.
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    ResetReply();
    SendChallengeFrom(web_state());
    EXPECT_FALSE(reply_received_);
  }
  EXPECT_EQ(held_callbacks.size(),
            DeviceTrustChallengeTabHelper::kMaxPendingRequests);

  // The (kMaxPendingRequests + 1)-th request must be rejected with
  // TOO_MANY_REQUESTS without reaching the service.
  ResetReply();
  base::RunLoop run_loop;
  SendChallengeFrom(web_state(), run_loop.QuitClosure());
  run_loop.Run();
  ExpectErrorReply("TOO_MANY_REQUESTS");
  ASSERT_TRUE(reply_.has_value());
  EXPECT_EQ(*reply_->GetDict().FindString("errorMessage"),
            "Too many pending device attestation requests.");

  // Complete held callbacks so pending requests are cleanly resolved.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "success";
  for (auto& callback : held_callbacks) {
    std::move(callback).Run(service_response);
  }

  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

// Verifies that reaching the pending requests limit on one WebState does not
// block attestation requests on a different WebState.
TEST_F(DeviceTrustJavaScriptFeatureTest,
       PendingRequestsIsolatedBetweenWebStates) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  auto second_web_state = std::make_unique<web::FakeWebState>();
  second_web_state->SetBrowserState(profile_.get());
  second_web_state->SetWebFramesManager(
      web::ContentWorld::kPageContentWorld,
      std::make_unique<web::FakeWebFramesManager>());
  DeviceTrustChallengeTabHelper::CreateForWebState(second_web_state.get());

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  ON_CALL(*mock_service(), Watches(testing::_))
      .WillByDefault(testing::Return(levels));

  std::vector<enterprise_connectors::DeviceTrustService::DeviceTrustCallback>
      held_callbacks;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .Times(DeviceTrustChallengeTabHelper::kMaxPendingRequests + 1)
      .WillRepeatedly(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { held_callbacks.push_back(std::move(callback)); });

  // Fill the first WebState up to the pending requests limit.
  for (size_t i = 0; i < DeviceTrustChallengeTabHelper::kMaxPendingRequests;
       ++i) {
    ResetReply();
    SendChallengeFrom(web_state());
    EXPECT_FALSE(reply_received_);
  }
  EXPECT_EQ(held_callbacks.size(),
            DeviceTrustChallengeTabHelper::kMaxPendingRequests);

  // The first WebState now feels the limit and rejects further requests.
  ResetReply();
  base::RunLoop run_loop;
  SendChallengeFrom(web_state(), run_loop.QuitClosure());
  run_loop.Run();
  ExpectErrorReply("TOO_MANY_REQUESTS");

  // The second WebState is isolated: its request is not blocked and succeeds
  // in reaching the service.
  ResetReply();
  SendChallengeFrom(second_web_state.get());
  EXPECT_FALSE(reply_received_);
  EXPECT_EQ(held_callbacks.size(),
            DeviceTrustChallengeTabHelper::kMaxPendingRequests + 1);

  // Complete held callbacks so pending requests are cleanly resolved.
  enterprise_connectors::DeviceTrustResponse service_response;
  service_response.challenge_response = "success";
  for (auto& callback : held_callbacks) {
    std::move(callback).Run(service_response);
  }

  DeviceTrustChallengeTabHelper::RemoveFromWebState(second_web_state.get());
  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

// Verifies that a valid message forwards the security origin, request URL,
// and challenge to the service, and resolves the promise with the signed
// payload.
TEST_F(DeviceTrustJavaScriptFeatureTest, DefaultRoutingResolvesSuccessPayload) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://example.com"));
  const GURL expected_url("https://example.com/login");

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  EXPECT_CALL(*mock_service(), Watches(expected_url))
      .WillOnce(testing::Return(levels));
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(kChallenge, levels, testing::_))
      .WillOnce(
          [](const std::string&,
             const std::set<enterprise_connectors::DTCPolicyLevel>&,
             enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                 callback) {
            enterprise_connectors::DeviceTrustResponse response;
            response.challenge_response = "signed_payload_abc";
            std::move(callback).Run(response);
          });

  ResetReply();
  web::ScriptMessage message(MakeMessageBody(base::Value(kChallenge)),
                             /*is_user_interacting=*/false,
                             /*is_main_frame=*/true,
                             /*request_url=*/expected_url,
                             /*security_origin=*/expected_origin);
  feature_.ScriptMessageReceivedWithReply(web_state(), message,
                                          CaptureReplyCallback());

  ASSERT_TRUE(reply_received_);
  ASSERT_TRUE(reply_.has_value());
  ASSERT_TRUE(reply_->is_dict());
  EXPECT_EQ(*reply_->GetDict().FindString("signedPayload"),
            "signed_payload_abc");
  EXPECT_EQ(reply_->GetDict().FindString("errorCode"), nullptr);
  EXPECT_EQ(reply_error_, nil);

  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

// Verifies that when the service takes longer than the timeout threshold to
// respond, the originating script message reply is rejected with
// ATTESTATION_TIMEOUT.
TEST_F(DeviceTrustJavaScriptFeatureTest, DefaultRoutingTimesOut) {
  DeviceTrustChallengeTabHelper::CreateForWebState(web_state());

  ON_CALL(*mock_service(), IsEnabled()).WillByDefault(testing::Return(true));
  const std::set<enterprise_connectors::DTCPolicyLevel> levels = {
      enterprise_connectors::DTCPolicyLevel::kUser};
  EXPECT_CALL(*mock_service(), Watches(GURL("https://example.com")))
      .WillOnce(testing::Return(levels));

  enterprise_connectors::DeviceTrustService::DeviceTrustCallback saved_callback;
  EXPECT_CALL(*mock_service(),
              BuildChallengeResponse(testing::_, levels, testing::_))
      .WillOnce(
          [&](const std::string&,
              const std::set<enterprise_connectors::DTCPolicyLevel>&,
              enterprise_connectors::DeviceTrustService::DeviceTrustCallback
                  callback) { saved_callback = std::move(callback); });

  ResetReply();
  web::ScriptMessage message(
      MakeMessageBody(base::Value(kChallenge)),
      /*is_user_interacting=*/false,
      /*is_main_frame=*/true,
      /*request_url=*/std::nullopt,
      /*security_origin=*/url::Origin::Create(GURL("https://example.com")));
  feature_.ScriptMessageReceivedWithReply(web_state(), message,
                                          CaptureReplyCallback());

  EXPECT_TRUE(saved_callback);
  EXPECT_FALSE(reply_received_);

  // Fast forward past the browser-side attestation timeout (25 seconds).
  task_environment_.FastForwardBy(base::Seconds(25));

  ExpectErrorReply("ATTESTATION_TIMEOUT");
  ASSERT_TRUE(reply_.has_value());
  EXPECT_EQ(*reply_->GetDict().FindString("errorMessage"),
            "Timed out waiting for attestation response.");

  // A late response from the service after timeout is safely ignored.
  enterprise_connectors::DeviceTrustResponse late_response;
  late_response.challenge_response = "late_payload";
  std::move(saved_callback).Run(late_response);

  DeviceTrustChallengeTabHelper::RemoveFromWebState(web_state());
}

}  // namespace
