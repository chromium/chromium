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
  WebViewWebStateMap* web_view_web_state_map =
      WebViewWebStateMap::FromBrowserState(GetBrowserState());
  ASSERT_TRUE(web_view_web_state_map);

  WKWebView* web_view = [[WKWebView alloc] init];
  WKWebView* web_view2 = [[WKWebView alloc] init];
  ASSERT_FALSE(web_view_web_state_map->GetWebStateForWebView(web_view));
  ASSERT_FALSE(web_view_web_state_map->GetWebStateForWebView(web_view2));

  FakeWebState web_state;
  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view, &web_state);
  FakeWebState web_state2;
  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view2,
                                                          &web_state2);

  EXPECT_EQ(&web_state,
            web_view_web_state_map->GetWebStateForWebView(web_view));
  EXPECT_EQ(&web_state2,
            web_view_web_state_map->GetWebStateForWebView(web_view2));
}

// Tests that a mapping is correctly updated as web view change for a given web
// state.
TEST_F(WebViewWebStateMapTest, UpdateMapping) {
  WebViewWebStateMap* web_view_web_state_map =
      WebViewWebStateMap::FromBrowserState(GetBrowserState());
  ASSERT_TRUE(web_view_web_state_map);

  FakeWebState web_state;
  WKWebView* web_view = [[WKWebView alloc] init];
  WKWebView* web_view2 = [[WKWebView alloc] init];

  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view, &web_state);
  EXPECT_EQ(&web_state,
            web_view_web_state_map->GetWebStateForWebView(web_view));

  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view2,
                                                          &web_state);
  EXPECT_EQ(&web_state,
            web_view_web_state_map->GetWebStateForWebView(web_view2));

  web_view_web_state_map->SetAssociatedWebViewForWebState(nil, &web_state);
  EXPECT_FALSE(web_view_web_state_map->GetWebStateForWebView(web_view2));

  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view, &web_state);
  EXPECT_EQ(&web_state,
            web_view_web_state_map->GetWebStateForWebView(web_view));
}

// Tests that mappings are removed when the web state is destroyed.
TEST_F(WebViewWebStateMapTest, WebStateDestroyed) {
  WebViewWebStateMap* web_view_web_state_map =
      WebViewWebStateMap::FromBrowserState(GetBrowserState());
  ASSERT_TRUE(web_view_web_state_map);

  WKWebView* web_view = [[WKWebView alloc] init];
  std::unique_ptr<FakeWebState> web_state = std::make_unique<FakeWebState>();
  web_view_web_state_map->SetAssociatedWebViewForWebState(web_view,
                                                          web_state.get());
  ASSERT_EQ(web_state.get(),
            web_view_web_state_map->GetWebStateForWebView(web_view));

  web_state.reset();

  EXPECT_FALSE(web_view_web_state_map->GetWebStateForWebView(web_view));
}

}  // namespace web
