// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_trust/device_trust_java_script_feature.h"

#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/script_message_value.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/origin.h"

namespace {

constexpr char kChallenge[] = "my_challenge";

std::unique_ptr<base::Value> MakeMessageLegacyBody(base::Value challenge) {
  base::DictValue body;
  body.Set("challengeRequest", std::move(challenge));
  return std::make_unique<base::Value>(std::move(body));
}

std::unique_ptr<web::ScriptMessageValue> MakeMessageBody(NSObject* challenge) {
  NSDictionary* body = @{@"challengeRequest" : challenge};
  return std::make_unique<web::ScriptMessageValue>(std::move(body));
}

// Subclass to intercept calls to HandleAttestationRequest in tests.
class TestDeviceTrustJavaScriptFeature : public DeviceTrustJavaScriptFeature {
 public:
  using DeviceTrustJavaScriptFeature::ScriptMessageReceivedWithReply;

  bool attestation_request_received() const {
    return attestation_request_received_;
  }

  const std::string& last_challenge() const { return last_challenge_; }

 protected:
  void HandleAttestationRequest(web::WebState* web_state,
                                const std::string& challenge_request,
                                ScriptMessageReplyCallback callback) override {
    attestation_request_received_ = true;
    last_challenge_ = challenge_request;
    std::move(callback).Run(nullptr, nil);
  }

 private:
  bool attestation_request_received_ = false;
  std::string last_challenge_;
};

// Test fixture verifying script message validation and routing.
class DeviceTrustJavaScriptFeatureTest : public IOSChromeTestWithWebState {
 protected:
  DeviceTrustJavaScriptFeatureTest()
      : IOSChromeTestWithWebState(
            IOSChromeTestWithWebState::WebClientMode::kChromeWebClient) {}

  base::OnceCallback<void(const base::Value*, NSString*)>
  CaptureReplyCallback() {
    // The test feature invokes reply callbacks synchronously, so this callback
    // cannot outlive the test fixture.
    return base::BindOnce(
        [](DeviceTrustJavaScriptFeatureTest* test, const base::Value* reply,
           NSString* error) {
          test->reply_received_ = true;
          if (reply) {
            test->reply_ = reply->Clone();
          }
          test->reply_error_ = error;
        },
        base::Unretained(this));
  }

  void SendMessage(std::unique_ptr<base::Value> legacy_body,
                   std::unique_ptr<web::ScriptMessageValue> body,
                   bool is_main_frame = true) {
    web::ScriptMessage message(std::move(legacy_body), std::move(body),
                               /*is_user_interacting=*/false, is_main_frame,
                               /*request_url=*/std::nullopt,
                               /*security_origin=*/url::Origin());
    feature_.ScriptMessageReceivedWithReply(web_state(), message,
                                            CaptureReplyCallback());
  }

  void SendChallenge(base::Value legacy_challenge, NSObject* challenge) {
    SendMessage(MakeMessageLegacyBody(std::move(legacy_challenge)),
                MakeMessageBody(std::move(challenge)));
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

  TestDeviceTrustJavaScriptFeature feature_;
  bool reply_received_ = false;
  std::optional<base::Value> reply_;
  NSString* reply_error_ = nil;
};

// Verifies that a well-formed message is forwarded to HandleAttestationRequest
// with the challenge string intact.
TEST_F(DeviceTrustJavaScriptFeatureTest, ValidRequestIsForwarded) {
  SendChallenge(base::Value(kChallenge), base::SysUTF8ToNSString(kChallenge));

  ASSERT_TRUE(feature_.attestation_request_received());
  EXPECT_EQ(feature_.last_challenge(), kChallenge);
}

// Verifies that messages originating from non-main frames are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectNonMainFrameMessage) {
  SendMessage(MakeMessageLegacyBody(base::Value(kChallenge)),
              MakeMessageBody(base::SysUTF8ToNSString(kChallenge)),
              /*is_main_frame=*/false);

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that messages with null bodies are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectMissingBody) {
  SendMessage(nullptr, nullptr);

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that non-dictionary message bodies are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectNonDictionaryBody) {
  SendMessage(std::make_unique<base::Value>("invalid"),
              std::make_unique<web::ScriptMessageValue>(@"invalid"));

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that messages missing the 'challengeRequest' key are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectMissingChallenge) {
  SendMessage(std::make_unique<base::Value>(base::DictValue()),
              std::make_unique<web::ScriptMessageValue>(@{}));

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that challenges of non-string types (e.g. integers) are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectWrongChallengeType) {
  SendChallenge(base::Value(42), @42);

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that empty challenge strings are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectEmptyChallenge) {
  SendChallenge(base::Value(""), @"");

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

// Verifies that challenges exceeding the maximum allowed size are rejected.
TEST_F(DeviceTrustJavaScriptFeatureTest, RejectOversizedChallenge) {
  std::string challenge = std::string(1025, 'a');
  SendChallenge(base::Value(challenge), base::SysUTF8ToNSString(challenge));

  EXPECT_FALSE(feature_.attestation_request_received());
  ExpectErrorReply("INVALID_CHALLENGE_REQUEST");
}

}  // namespace
