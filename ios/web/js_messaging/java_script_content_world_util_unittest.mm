// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world_util.h"

#import <WebKit/WebKit.h>

#import "testing/platform_test.h"

namespace web {

using JavaScriptContentWorldUtilsTest = PlatformTest;

// Tests the mapping from a WKContentWorld to a web::ContentWorld enum value.
TEST_F(JavaScriptContentWorldUtilsTest, WKContentWorldToContentWorldMapping) {
  EXPECT_EQ(ContentWorld::kPageContentWorld,
            ContentWorldIdentifierForWKContentWorld(WKContentWorld.pageWorld));
  EXPECT_EQ(ContentWorld::kIsolatedWorld,
            ContentWorldIdentifierForWKContentWorld(
                WKContentWorld.defaultClientWorld));
}

// Tests the mapping from a web::ContentWorld enum value to a WKContentWorld.
TEST_F(JavaScriptContentWorldUtilsTest, ContentWorldToWKContentWorldMapping) {
  EXPECT_EQ(WKContentWorld.pageWorld, WKContentWorldForContentWorldIdentifier(
                                          ContentWorld::kPageContentWorld));
  EXPECT_EQ(
      WKContentWorld.defaultClientWorld,
      WKContentWorldForContentWorldIdentifier(ContentWorld::kIsolatedWorld));
}

}  // namespace web