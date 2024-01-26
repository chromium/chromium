// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"

#import <memory>

#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  explicit FakeNavigationManager(int last_committed_item_index)
      : last_committed_item_index_(last_committed_item_index) {}

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override {
    return last_committed_item_index_;
  }

 private:
  int last_committed_item_index_;
};

}  // namespace

class WebStateOpenerTest : public PlatformTest {
 public:
  WebStateOpenerTest() = default;

  WebStateOpenerTest(const WebStateOpenerTest&) = delete;
  WebStateOpenerTest& operator=(const WebStateOpenerTest&) = delete;

  std::unique_ptr<web::WebState> CreateWebState(int last_committed_item_index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>(last_committed_item_index));
    return web_state;
  }
};

TEST_F(WebStateOpenerTest, NullWebState) {
  WebStateOpener opener(nullptr);

  EXPECT_EQ(nullptr, opener.opener.get());
  EXPECT_EQ(-1, opener.navigation_index);
}

TEST_F(WebStateOpenerTest, DefaultNavigationIndex) {
  std::unique_ptr<web::WebState> web_state = CreateWebState(2);
  WebStateOpener opener(web_state.get());

  EXPECT_EQ(web_state.get(), opener.opener);
  EXPECT_EQ(2, opener.navigation_index);
}

TEST_F(WebStateOpenerTest, ExplicitNavigationIndex) {
  std::unique_ptr<web::WebState> web_state = CreateWebState(2);
  WebStateOpener opener(web_state.get(), 1);

  EXPECT_EQ(web_state.get(), opener.opener);
  EXPECT_EQ(1, opener.navigation_index);
}
