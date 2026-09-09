// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/password_protection_java_script_feature.h"

#import <memory>
#import <vector>

#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/safe_browsing/model/input_event_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
    key_pressed_count_++;
  }
  void OnPaste(std::string text) override {
    on_paste_called_ = true;
    pasted_text_ = text;
  }
  void OnPasteKeyDetected() override { on_paste_key_detected_called_ = true; }
  web::WebState* web_state() const override { return web_state_; }

  bool on_key_pressed_called_ = false;
  int key_pressed_count_ = 0;
  bool on_paste_called_ = false;
  bool on_paste_key_detected_called_ = false;
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

// Tests that KeyDown events are correctly checked for a single Unicode code
// point.
TEST_F(PasswordProtectionJavaScriptFeatureTest, KeyDownEventLengthCheck) {
  // A single ASCII character (1 code point, 1 code unit in UTF-16).
  base::Value body1(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "a"));
  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(&web_state_, message1);
  EXPECT_TRUE(observer_->on_key_pressed_called_);
  observer_->on_key_pressed_called_ = false;

  // Advance time by keydown rate limit.
  task_environment_.FastForwardBy(base::Milliseconds(25));

  // A single supplementary Unicode code point (e.g., U+1F600 Grinning Face
  // emoji). It takes 2 UTF-16 code units (surrogate pair) but is 1 Unicode code
  // point.
  base::Value body2(base::DictValue()
                        .Set("eventType", "KeyDown")
                        .Set("text", "\xF0\x9F\x98\x80"));
  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(&web_state_, message2);
  EXPECT_TRUE(observer_->on_key_pressed_called_);
  observer_->on_key_pressed_called_ = false;

  // Advance time by keydown rate limit.
  task_environment_.FastForwardBy(base::Milliseconds(25));

  // Multiple characters should be dropped.
  base::Value body3(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "ab"));
  web::ScriptMessage message3(std::make_unique<base::Value>(std::move(body3)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(&web_state_, message3);
  EXPECT_FALSE(observer_->on_key_pressed_called_);
}

// Tests that a paste key detected event is forwarded to the observer after the
// coalescing timer.
TEST_F(PasswordProtectionJavaScriptFeatureTest,
       PasteKeyDetectedEventForwarded) {
  base::Value body(base::DictValue().Set("eventType", "PasteKeyDetected"));

  web::ScriptMessage message(std::make_unique<base::Value>(std::move(body)),
                             /*is_user_interacting=*/true,
                             /*is_main_frame=*/true,
                             /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message);

  // Key event is queued, not yet forwarded.
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);

  // Advance time by 100ms for the coalescing timer.
  task_environment_.FastForwardBy(base::Milliseconds(100));

  EXPECT_TRUE(observer_->on_paste_key_detected_called_);
}

// Tests that paste key detected events are rate limited.
TEST_F(PasswordProtectionJavaScriptFeatureTest,
       PasteKeyDetectedEventRateLimited) {
  base::Value body1(base::DictValue().Set("eventType", "PasteKeyDetected"));

  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  // First paste key event should be allowed and start the timer.
  feature_->ScriptMessageReceived(&web_state_, message1);
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);

  // Fast forward by 100ms. Timer fires.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(observer_->on_paste_key_detected_called_);
  observer_->on_paste_key_detected_called_ = false;

  // Second paste key event immediately after (elapsed 100ms since first)
  // should be dropped.
  base::Value body2(base::DictValue().Set("eventType", "PasteKeyDetected"));

  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(&web_state_, message2);
  // Fast forward by 100ms. Timer should not fire since the event was dropped.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);

  // Advance time to 300ms since first event (elapsed > 200ms).
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Third paste key event should be allowed.
  feature_->ScriptMessageReceived(&web_state_, message2);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(observer_->on_paste_key_detected_called_);
}

// Tests that paste key detected event is cancelled if a text pasted event
// arrives within the coalescing window.
TEST_F(PasswordProtectionJavaScriptFeatureTest,
       PasteKeyDetectedAndTextPastedCoalescing) {
  base::Value body1(base::DictValue().Set("eventType", "PasteKeyDetected"));

  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  // Paste key event should be allowed and start timer.
  feature_->ScriptMessageReceived(&web_state_, message1);
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);

  // Text pasted event immediately after should cancel the timer and trigger
  // OnPaste immediately.
  base::Value body2(base::DictValue()
                        .Set("eventType", "TextPasted")
                        .Set("text", "password1"));

  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message2);
  EXPECT_TRUE(observer_->on_paste_called_);

  // Fast forward 200ms to let any timer expire. OnPasteKeyDetected should
  // NOT be called.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);
}

// Tests that key down events are rate limited.
TEST_F(PasswordProtectionJavaScriptFeatureTest, KeyDownEventRateLimited) {
  base::Value body1(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "a"));
  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  // First key down should be allowed.
  feature_->ScriptMessageReceived(&web_state_, message1);
  EXPECT_TRUE(observer_->on_key_pressed_called_);
  observer_->on_key_pressed_called_ = false;

  // Second key down immediately after should be dropped.
  base::Value body2(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "b"));
  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message2);
  EXPECT_FALSE(observer_->on_key_pressed_called_);

  // Third key down should be allowed after a sufficient amount of time.
  base::Value body3(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "b"));
  web::ScriptMessage message3(std::make_unique<base::Value>(std::move(body3)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());
  task_environment_.FastForwardBy(base::Milliseconds(25));
  feature_->ScriptMessageReceived(&web_state_, message3);
  EXPECT_TRUE(observer_->on_key_pressed_called_);
}

// Tests that if the text pasted event arrives after the coalescing window
// has expired (and UIPasteboard has already been read), the text pasted event
// is ignored (rate limited).
TEST_F(PasswordProtectionJavaScriptFeatureTest,
       PasteKeyDetectedAndTextPastedLateArriving) {
  base::Value body1(base::DictValue().Set("eventType", "PasteKeyDetected"));

  web::ScriptMessage message1(std::make_unique<base::Value>(std::move(body1)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  // Paste key event starts the timer.
  feature_->ScriptMessageReceived(&web_state_, message1);

  // Fast forward 100ms. Timer expires and triggers OnPasteKeyDetected.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(observer_->on_paste_key_detected_called_);

  // Text pasted event arrives 50ms later (total 150ms elapsed, which is < 200ms
  // rate limit).
  base::Value body2(base::DictValue()
                        .Set("eventType", "TextPasted")
                        .Set("text", "password1"));

  web::ScriptMessage message2(std::make_unique<base::Value>(std::move(body2)),
                              /*is_user_interacting=*/true,
                              /*is_main_frame=*/true,
                              /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message2);
  // It should be rate limited since we already processed the paste.
  EXPECT_FALSE(observer_->on_paste_called_);
}

// Tests that paste key detected events are ignored when the feature flag is
// disabled.
TEST_F(PasswordProtectionJavaScriptFeatureTest, PasteKeyDetectedDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kIOSPhishGuardPasteShortcutDetection);

  base::Value body(base::DictValue().Set("eventType", "PasteKeyDetected"));
  web::ScriptMessage message(std::make_unique<base::Value>(std::move(body)),
                             /*is_user_interacting=*/true,
                             /*is_main_frame=*/true,
                             /*request_url=*/std::nullopt, url::Origin());

  feature_->ScriptMessageReceived(&web_state_, message);

  // Fast forward by 200ms (twice the timer duration). Timer should NOT fire,
  // and OnPasteKeyDetected should NOT be called.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_FALSE(observer_->on_paste_key_detected_called_);
}

// Tests that keydown events across multiple WebStates are rate-limited in
// aggregate.
TEST_F(PasswordProtectionJavaScriptFeatureTest,
       KeyDownAggregateRateLimitedAcrossMultipleWebStates) {
  constexpr int kNumExtraWebStates = 4;
  std::vector<std::unique_ptr<web::FakeWebState>> extra_web_states;
  std::vector<std::unique_ptr<MockInputEventObserver>> extra_observers;
  std::vector<web::WebState*> all_web_states;
  std::vector<MockInputEventObserver*> all_observers;

  all_web_states.push_back(&web_state_);
  all_observers.push_back(observer_.get());

  for (int i = 0; i < kNumExtraWebStates; ++i) {
    auto ws = std::make_unique<web::FakeWebState>();
    auto obs = std::make_unique<MockInputEventObserver>(ws.get());
    feature_->AddObserver(obs.get());
    all_web_states.push_back(ws.get());
    all_observers.push_back(obs.get());
    extra_web_states.push_back(std::move(ws));
    extra_observers.push_back(std::move(obs));
  }

  // Send 80 keydown events round-robin across all WebStates.
  // With 5 WebStates and 10ms between events, each WebState receives an event
  // every 50ms (>= 25ms per-WebState limit), and 80 events * 10ms = 800ms total
  // (< 1s aggregate window limit).
  for (int i = 0; i < 80; ++i) {
    web::WebState* target_ws = all_web_states[i % all_web_states.size()];
    base::Value body(
        base::DictValue().Set("eventType", "KeyDown").Set("text", "a"));
    web::ScriptMessage message(std::make_unique<base::Value>(std::move(body)),
                               /*is_user_interacting=*/true,
                               /*is_main_frame=*/true,
                               /*request_url=*/std::nullopt, url::Origin());
    feature_->ScriptMessageReceived(target_ws, message);
    task_environment_.FastForwardBy(base::Milliseconds(10));
  }

  int total_key_presses = 0;
  for (auto* obs : all_observers) {
    total_key_presses += obs->key_pressed_count_;
  }
  EXPECT_EQ(total_key_presses, 80);

  // The 81st event within the same 1-second window should be dropped by the
  // aggregate rate limit.
  base::Value body_extra(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "b"));
  web::ScriptMessage message_extra(
      std::make_unique<base::Value>(std::move(body_extra)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(all_web_states[0], message_extra);

  total_key_presses = 0;
  for (auto* obs : all_observers) {
    total_key_presses += obs->key_pressed_count_;
  }
  EXPECT_EQ(total_key_presses, 80);

  // Fast forward by 1 second so the aggregate rate limit window resets.
  task_environment_.FastForwardBy(base::Seconds(1));

  // The next keydown should now be allowed.
  base::Value body_after(
      base::DictValue().Set("eventType", "KeyDown").Set("text", "c"));
  web::ScriptMessage message_after(
      std::make_unique<base::Value>(std::move(body_after)),
      /*is_user_interacting=*/true,
      /*is_main_frame=*/true,
      /*request_url=*/std::nullopt, url::Origin());
  feature_->ScriptMessageReceived(all_web_states[0], message_after);

  total_key_presses = 0;
  for (auto* obs : all_observers) {
    total_key_presses += obs->key_pressed_count_;
  }
  EXPECT_EQ(total_key_presses, 81);

  for (auto& obs : extra_observers) {
    feature_->RemoveObserver(obs.get());
  }
}

}  // namespace
