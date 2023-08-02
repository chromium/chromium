// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#import "base/test/gtest_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

namespace web {

typedef WebTest JavaScriptContentWorldTest;

// Tests adding a JavaScriptFeature which only supports the page content world.
TEST_F(JavaScriptContentWorldTest, AddPageContentWorldFeature) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.pageWorld);

  FakeJavaScriptFeature feature(ContentWorld::kPageContentWorld);
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));

  EXPECT_EQ(WKContentWorld.pageWorld, world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  // Two scripts are added by FakeJavaScriptFeature.
  EXPECT_EQ(initial_scripts_count + 2, scripts_count);
}

// Tests adding a JavaScriptFeature which only supports the isolated world.
TEST_F(JavaScriptContentWorldTest, AddIsolatedWorldFeature) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.defaultClientWorld);

  FakeJavaScriptFeature feature(ContentWorld::kIsolatedWorld);
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));

  EXPECT_EQ(WKContentWorld.defaultClientWorld, world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  // Two scripts are added by FakeJavaScriptFeature.
  EXPECT_EQ(initial_scripts_count + 2, scripts_count);
}

// Tests adding a JavaScriptFeature which supports all content worlds.
TEST_F(JavaScriptContentWorldTest, AddAllContentWorldsFeature) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  FakeJavaScriptFeature feature(ContentWorld::kAllContentWorlds);

  web::JavaScriptContentWorld isolated_world(GetBrowserState(),
                                             WKContentWorld.defaultClientWorld);
  isolated_world.AddFeature(&feature);
  EXPECT_TRUE(isolated_world.HasFeature(&feature));
  EXPECT_EQ(WKContentWorld.defaultClientWorld,
            isolated_world.GetWKContentWorld());

  web::JavaScriptContentWorld page_world(GetBrowserState(),
                                         WKContentWorld.pageWorld);
  page_world.AddFeature(&feature);
  EXPECT_TRUE(page_world.HasFeature(&feature));
  EXPECT_EQ(WKContentWorld.pageWorld, page_world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  // Two scripts are added by FakeJavaScriptFeature, but user_content_controller
  // should now have four additional scripts, two for each world.
  EXPECT_EQ(initial_scripts_count + 4, scripts_count);
}

}  // namespace web
