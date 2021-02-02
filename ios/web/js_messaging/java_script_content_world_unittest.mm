// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#import "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

typedef WebTest JavaScriptContentWorldTest;

// Tests adding a JavaScriptFeature to a JavaScriptContentWorld adds the
// expected user scripts and script message handlers.
TEST_F(JavaScriptContentWorldTest, AddFeature) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  web::JavaScriptContentWorld world(GetBrowserState());

  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld);
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    EXPECT_FALSE(world.GetWKContentWorld());
  }
#endif  // defined(__IPHONE14_0)

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  ASSERT_GT(scripts_count, initial_scripts_count);
}

// Tests adding a JavaScriptFeature to a specific JavaScriptContentWorld.
TEST_F(JavaScriptContentWorldTest, AddFeatureToSpecificWKContentWorld) {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
    WKUserContentController* user_content_controller =
        configuration_provider.GetWebViewConfiguration().userContentController;

    unsigned long initial_scripts_count =
        [[user_content_controller userScripts] count];
    ASSERT_GT(initial_scripts_count, 0ul);

    web::JavaScriptContentWorld world(GetBrowserState(),
                                      [WKContentWorld defaultClientWorld]);

    FakeJavaScriptFeature feature(
        JavaScriptFeature::ContentWorld::kAnyContentWorld);
    world.AddFeature(&feature);
    EXPECT_TRUE(world.HasFeature(&feature));

    EXPECT_EQ([WKContentWorld defaultClientWorld], world.GetWKContentWorld());

    unsigned long scripts_count = [[user_content_controller userScripts] count];
    ASSERT_GT(scripts_count, initial_scripts_count);
  }
#endif  // defined(__IPHONE14_0)
}

}  // namespace web
