// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#import "ios/web/public/js_messaging/java_script_feature.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest JavaScriptContentWorldTest;

// Tests adding a JavaScriptFeature to a JavaScriptContentWorld.
TEST_F(JavaScriptContentWorldTest, AddFeature) {
  WKUserContentController* user_content_controller =
      [[WKUserContentController alloc] init];
  web::JavaScriptContentWorld world(user_content_controller);

  const web::JavaScriptFeature& feature = web::JavaScriptFeature(
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld, {});
  world.AddFeature(&feature);
  EXPECT_TRUE(world.HasFeature(&feature));
}

// Tests adding a JavaScriptFeature to a specific JavaScriptContentWorld.
TEST_F(JavaScriptContentWorldTest, AddFeatureToSpecificWKContentWorld) {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    WKUserContentController* user_content_controller =
        [[WKUserContentController alloc] init];
    web::JavaScriptContentWorld world(user_content_controller,
                                      [WKContentWorld defaultClientWorld]);

    const web::JavaScriptFeature& feature = web::JavaScriptFeature(
        web::JavaScriptFeature::ContentWorld::kAnyContentWorld, {});
    world.AddFeature(&feature);
    EXPECT_TRUE(world.HasFeature(&feature));
  }
#endif  // defined(__IPHONE14_0)
}
