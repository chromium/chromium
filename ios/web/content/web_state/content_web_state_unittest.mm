// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state.h"

#import "content/public/browser/visibility.h"
#import "content/public/browser/web_contents.h"
#import "content/public/test/browser_task_environment.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/web_client.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

class ContentWebStateTest : public PlatformTest {
 public:
  void SetUp() override {
    web::SetWebClient(&web_client_);
    content_web_state_ = std::make_unique<ContentWebState>(
        WebState::CreateParams(browser_state()));
  }

  FakeBrowserState* browser_state() { return &browser_state_; }
  ContentWebState* content_web_state() { return content_web_state_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebClient web_client_;
  // The web state for testing.
  std::unique_ptr<ContentWebState> content_web_state_;
};

// Test if WasShown/WasHidden/IsVisible methods work correctly when a web
// contents's visibility is changed.
TEST_F(ContentWebStateTest, VisibilityTest) {
  auto observer = std::make_unique<FakeWebStateObserver>(content_web_state());
  EXPECT_EQ(content_web_state(), observer->web_state());
  EXPECT_TRUE(content_web_state()->IsVisible());

  // Check if the observer receives the web state, and the IsVisible returns
  // false.
  content_web_state()->GetWebContents()->WasHidden();
  ASSERT_TRUE(observer->was_hidden_info());
  EXPECT_EQ(content_web_state(), observer->was_hidden_info()->web_state);
  EXPECT_FALSE(content_web_state()->IsVisible());

  // Check if the observer receives the web state, and the IsVisible returns
  // true.
  content_web_state()->GetWebContents()->WasShown();
  ASSERT_TRUE(observer->was_shown_info());
  EXPECT_EQ(content_web_state(), observer->was_shown_info()->web_state);
  EXPECT_TRUE(content_web_state()->IsVisible());
}

// Tests that setting and getting user agent override works.
TEST_F(ContentWebStateTest, UserAgentOverride) {
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());
  std::string ua_override = "Fake UA String";
  content_web_state()->SetUserAgentOverride(ua_override);
  EXPECT_EQ(ua_override, content_web_state()->GetUserAgentOverride().value());

  content_web_state()->SetUserAgentOverride(std::nullopt);
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());

  content_web_state()->SetUserAgentOverride(ua_override);
  EXPECT_TRUE(content_web_state()->GetUserAgentOverride().has_value());

  // An explicit empty string is treated as no override (std::nullopt).
  content_web_state()->SetUserAgentOverride("");
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());
}

// Tests that setting an invalid user agent override is ignored.
TEST_F(ContentWebStateTest, UserAgentOverrideValidation) {
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());

  // String with a newline is an invalid header value.
  std::string invalid_ua = "Fake\nUA";
  content_web_state()->SetUserAgentOverride(invalid_ua);
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());

  // Normal string should still work.
  std::string valid_ua = "Fake UA";
  content_web_state()->SetUserAgentOverride(valid_ua);
  EXPECT_EQ(valid_ua, content_web_state()->GetUserAgentOverride().value());

  // Clearing still works.
  content_web_state()->SetUserAgentOverride(std::nullopt);
  EXPECT_FALSE(content_web_state()->GetUserAgentOverride().has_value());
}

// Tests that the web state has an opener after calling SetHasOpener().
TEST_F(ContentWebStateTest, SetHasOpener) {
  ASSERT_FALSE(content_web_state()->HasOpener());
  content_web_state()->SetHasOpener(true);
  EXPECT_TRUE(content_web_state()->HasOpener());
}

}  // namespace web
