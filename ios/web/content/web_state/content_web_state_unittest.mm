// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/web_state/content_web_state.h"

#import "content/public/browser/visibility.h"
#import "content/public/browser/web_contents.h"
#import "content/public/test/browser_task_environment.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/web_client.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

class ContentWebStateTest : public PlatformTest {
 public:
  void SetUp() override { web::SetWebClient(&web_client_); }

  FakeBrowserState* browser_state() { return &browser_state_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  FakeWebClient web_client_;
};

// Test if WasShown/WasHidden/IsVisible methods work correctly when a web
// contents's visibility is changed.
TEST_F(ContentWebStateTest, VisibilityTest) {
  std::unique_ptr<ContentWebState> web_state =
      std::make_unique<ContentWebState>(
          WebState::CreateParams(browser_state()));

  auto observer = std::make_unique<FakeWebStateObserver>(web_state.get());
  EXPECT_EQ(web_state.get(), observer->web_state());
  EXPECT_TRUE(web_state->IsVisible());

  // Check if the observer receives the web state, and the IsVisible returns
  // false.
  web_state->GetWebContents()->WasHidden();
  ASSERT_TRUE(observer->was_hidden_info());
  EXPECT_EQ(web_state.get(), observer->was_hidden_info()->web_state);
  EXPECT_FALSE(web_state->IsVisible());

  // Check if the observer receives the web state, and the IsVisible returns
  // true.
  web_state->GetWebContents()->WasShown();
  ASSERT_TRUE(observer->was_shown_info());
  EXPECT_EQ(web_state.get(), observer->was_shown_info()->web_state);
  EXPECT_TRUE(web_state->IsVisible());
}

}  // namespace web
