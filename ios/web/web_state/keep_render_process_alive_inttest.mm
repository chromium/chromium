// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#include <functional>

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
// Overrides GetWindowedContainer.
class FakeWebClient : public WebClient {
  UIView* GetWindowedContainer() override {
    if (!windowed_container)
      windowed_container = [[UIView alloc] init];
    return windowed_container;
  }

  UIView* windowed_container = nil;
};
}  // namespace

// Test fixture for the KeepRenderProcessAlive flag.
class KeepRenderProcessAliveTest : public WebTestWithWebState {
 protected:
  KeepRenderProcessAliveTest()
      : WebTestWithWebState(std::make_unique<FakeWebClient>()) {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kKeepsRenderProcessAlive);
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();
    ASSERT_EQ(0U, GetWebClient()->GetWindowedContainer().subviews.count);
    ASSERT_TRUE(LoadHtml("<body></body>"));
    // WebTestWithWebState adds the view to thie view hierarchy.  Remove it for
    // the beginning of each test.
    [web_state()->GetView() removeFromSuperview];
  }

  bool WindowedContainerHasSubview() {
    return GetWebClient()->GetWindowedContainer().subviews.count == 1;
  }

  bool IsSubviewOfKeyWindow(UIView* view) {
    return view.superview == GetKeyWindow();
  }

  UIWindow* GetKeyWindow() {
    return [UIApplication sharedApplication].keyWindow;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(KeepRenderProcessAliveTest);
};

// Test's that nothing is added to the WindowedContainer when
// -KeepRenderProcessAlive is disabled.
TEST_F(KeepRenderProcessAliveTest, KeepRenderProcessAliveOff) {
  web_state()->SetKeepRenderProcessAlive(false);
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));

  [GetKeyWindow() addSubview:web_state()->GetView()];

  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_TRUE(IsSubviewOfKeyWindow(web_state()->GetView()));

  [web_state()->GetView() removeFromSuperview];
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));
}

// Test's that the webState view is added to the windowed container when
// -KeepRenderProcessAlive is enabled and the the webState view has no window,
// and removed when the webState is added back to a window.
TEST_F(KeepRenderProcessAliveTest, KeepRenderProcessAliveOn) {
  web_state()->SetKeepRenderProcessAlive(true);
  ASSERT_TRUE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));

  [GetKeyWindow() addSubview:web_state()->GetView()];
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_TRUE(IsSubviewOfKeyWindow(web_state()->GetView()));

  [web_state()->GetView() removeFromSuperview];
  ASSERT_TRUE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));
}

// Test's that the webState view is added or removed from the windowed
// container when -KeepRenderProcessAlive is enabled or disabled, respectively.
TEST_F(KeepRenderProcessAliveTest, KeepRenderProcessAliveToggle) {
  web_state()->SetKeepRenderProcessAlive(true);
  ASSERT_TRUE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));

  web_state()->SetKeepRenderProcessAlive(false);
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));

  web_state()->SetKeepRenderProcessAlive(true);
  [GetKeyWindow() addSubview:web_state()->GetView()];
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_TRUE(IsSubviewOfKeyWindow(web_state()->GetView()));

  [web_state()->GetView() removeFromSuperview];
  ASSERT_TRUE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));

  web_state()->SetKeepRenderProcessAlive(false);
  ASSERT_FALSE(WindowedContainerHasSubview());
  ASSERT_FALSE(IsSubviewOfKeyWindow(web_state()->GetView()));
}

}  // namespace web
