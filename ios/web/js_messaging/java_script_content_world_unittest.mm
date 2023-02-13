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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

typedef WebTest JavaScriptContentWorldTest;

// Tests that adding a JavaScriptFeature to a JavaScriptContentWorld adds the
// expected user scripts and script message handlers. Also verifies that
// features created with kAnyContentWorld can be added to the page content
// world.
TEST_F(JavaScriptContentWorldTest, AddFeature) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.pageWorld);

  FakeJavaScriptFeature feature(ContentWorld::kAnyContentWorld);
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));
  EXPECT_EQ(WKContentWorld.pageWorld, world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  ASSERT_GT(scripts_count, initial_scripts_count);
}

// Tests adding a JavaScriptFeature to a specific JavaScriptContentWorld. Also
// verifies that features created with kAnyContentWorld can be added to an
// isolated world.
TEST_F(JavaScriptContentWorldTest, AddFeatureToSpecificWKContentWorld) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  unsigned long initial_scripts_count =
      [[user_content_controller userScripts] count];
  ASSERT_GT(initial_scripts_count, 0ul);

  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.defaultClientWorld);

  FakeJavaScriptFeature feature(ContentWorld::kAnyContentWorld);
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));

  EXPECT_EQ(WKContentWorld.defaultClientWorld, world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  ASSERT_GT(scripts_count, initial_scripts_count);
}

// Tests adding a JavaScriptFeature which only supports isolated worlds.
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
  ASSERT_GT(scripts_count, initial_scripts_count);
}

// Tests that attempting to add a JavaScriptFeature which only supports an
// isolated world to the page content world triggers a DCHECK.
TEST_F(JavaScriptContentWorldTest,
       AttemptAddingIsolatedWorldFeatureToPageWorld) {
  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.pageWorld);
  FakeJavaScriptFeature feature(ContentWorld::kIsolatedWorld);

  EXPECT_DCHECK_DEATH(world.AddFeature(&feature));
}

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

  EXPECT_NSEQ(WKContentWorld.pageWorld, world.GetWKContentWorld());

  unsigned long scripts_count = [[user_content_controller userScripts] count];
  ASSERT_GT(scripts_count, initial_scripts_count);
}

// Tests that attempting to add a JavaScriptFeature which only supports the
// page content world to an isolated world triggers a DCHECK.
TEST_F(JavaScriptContentWorldTest,
       AttemptAddingPageContentWorldFeatureToIsolatedWorld) {
  web::JavaScriptContentWorld world(GetBrowserState(),
                                    WKContentWorld.defaultClientWorld);
  FakeJavaScriptFeature feature(ContentWorld::kPageContentWorld);

  EXPECT_DCHECK_DEATH(world.AddFeature(&feature));
}

}  // namespace web
