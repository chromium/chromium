// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_web_state_map.h"

#import <WebKit/WebKit.h>

#import <memory>

#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"

namespace web {

typedef WebTest WebViewWebStateMapTest;

// Tests that web views are correctly mapped to web states.
TEST_F(WebViewWebStateMapTest, CreateMappings) {
  WKWebView* web_view = [[WKWebView alloc] init];
  WKWebView* web_view2 = [[WKWebView alloc] init];
  ASSERT_FALSE(GetWebStateForWebView(web_view));
  ASSERT_FALSE(GetWebStateForWebView(web_view2));

  FakeWebState web_state;
  SetAssociatedWebViewForWebState(web_view, &web_state);

  FakeWebState web_state2;
  SetAssociatedWebViewForWebState(web_view2, &web_state2);

  EXPECT_EQ(&web_state, GetWebStateForWebView(web_view));
  EXPECT_EQ(&web_state2, GetWebStateForWebView(web_view2));
}

// Tests that a mapping is correctly updated as web view change for a given web
// state.
TEST_F(WebViewWebStateMapTest, UpdateMapping) {
  FakeWebState web_state;
  WKWebView* web_view = [[WKWebView alloc] init];
  WKWebView* web_view2 = [[WKWebView alloc] init];

  SetAssociatedWebViewForWebState(web_view, &web_state);
  EXPECT_EQ(&web_state, GetWebStateForWebView(web_view));
  EXPECT_FALSE(GetWebStateForWebView(web_view2));

  SetAssociatedWebViewForWebState(web_view2, &web_state);
  EXPECT_EQ(&web_state, GetWebStateForWebView(web_view2));
  EXPECT_EQ(&web_state, GetWebStateForWebView(web_view));

  ClearAssociatedWebViewForWebState(web_view2, &web_state);
  EXPECT_EQ(&web_state, GetWebStateForWebView(web_view));
  EXPECT_FALSE(GetWebStateForWebView(web_view2));

  ClearAssociatedWebViewForWebState(web_view, &web_state);
  EXPECT_FALSE(GetWebStateForWebView(web_view));
}

// Tests that mappings are removed when the web state is destroyed.
TEST_F(WebViewWebStateMapTest, WebStateDestroyed) {
  WKWebView* web_view = [[WKWebView alloc] init];
  std::unique_ptr<FakeWebState> web_state = std::make_unique<FakeWebState>();
  SetAssociatedWebViewForWebState(web_view, web_state.get());
  ASSERT_EQ(web_state.get(), GetWebStateForWebView(web_view));

  web_state.reset();

  EXPECT_FALSE(GetWebStateForWebView(web_view));
}

// Tests that call GetWebStateForWebView(...) with nil is supported.
TEST_F(WebViewWebStateMapTest, AcceptNil) {
  EXPECT_EQ(GetWebStateForWebView(nil), nullptr);
}

}  // namespace web
