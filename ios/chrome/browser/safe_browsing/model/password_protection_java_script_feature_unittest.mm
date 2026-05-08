// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"

#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/safe_browsing/model/input_event_observer.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class MockInputEventObserver : public InputEventObserver {
 public:
  explicit MockInputEventObserver(web::WebState* web_state)
      : web_state_(web_state) {}
  virtual ~MockInputEventObserver() = default;
  void OnKeyPressed(std::string text) override {
    on_key_pressed_called_ = true;
  }
  void OnPaste(std::string text) override {
    on_paste_called_ = true;
    pasted_text_ = text;
  }
  web::WebState* web_state() const override { return web_state_; }

  bool on_key_pressed_called_ = false;
  bool on_paste_called_ = false;
  std::string pasted_text_;
  raw_ptr<web::WebState> web_state_;
};

class PasswordProtectionJavaScriptFeatureTest : public PlatformTest {
 protected:
  PasswordProtectionJavaScriptFeatureTest()
      : task_environment_(web::WebTaskEnvironment::TimeSource::MOCK_TIME),
        feature_(PasswordProtectionJavaScriptFeature::GetInstance()) {}

  void SetUp() override {
    PlatformTest::SetUp();
    observer_ = std::make_unique<MockInputEventObserver>(&web_state_);
    feature_->AddObserver(observer_.get());
  }

  void TearDown() override {
    feature_->RemoveObserver(observer_.get());
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  raw_ptr<PasswordProtectionJavaScriptFeature> feature_;
  std::unique_ptr<MockInputEventObserver> observer_;
};

// Tests that a normal paste event is forwarded to the observer.
TEST_F(PasswordProtectionJavaScriptFeatureTest, PasteEventForwarded) {
  base::Value body(base::DictValue()
                       .Set("eventType", "TextPasted")
                       .Set("text", "normal_password"));

  web::ScriptMessage message(std::make_unique<base::Value>(std::move(body)),
                             /*is_user_interacting=*/true,
                             /*is_main_frame=*/true,
                             /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message);

  EXPECT_TRUE(observer_->on_paste_called_);
  EXPECT_EQ(observer_->pasted_text_, "normal_password");
}

// Tests that paste events are rate limited.
TEST_F(PasswordProtectionJavaScriptFeatureTest, PasteEventRateLimited) {
  base::Value body1(base::DictValue()
                        .Set("eventType", "TextPasted")
                        .Set("text", "password1"));

  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  // First paste should be allowed.
  feature_->ScriptMessageReceived(&web_state_, message1);
  EXPECT_TRUE(observer_->on_paste_called_);
  observer_->on_paste_called_ = false;

  // Second paste immediately after should be dropped.
  base::Value body2(base::DictValue()
                        .Set("eventType", "TextPasted")
                        .Set("text", "password2"));

  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message2);
  EXPECT_FALSE(observer_->on_paste_called_);

  // Advance time by 250ms (greater than 200ms limit).
  task_environment_.FastForwardBy(base::Milliseconds(250));

  // Third paste should be allowed.
  feature_->ScriptMessageReceived(&web_state_, message2);
  EXPECT_TRUE(observer_->on_paste_called_);
}

}  // namespace
