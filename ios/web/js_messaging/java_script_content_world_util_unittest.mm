// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world_util.h"

#import <WebKit/WebKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/test/js_test_util_internal.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace web {

namespace {

// HTML with named elements that shadow `document.forms` and `form.elements`
// when legacy built-in overrides are enabled.
NSString* const kClobberingHtml =
    @"<form name='forms'></form>"
     "<form id='f'><input name='elements'></form>";

// Evaluates to the WebIDL class name of `document.forms`.
NSString* const kDocumentFormsClassScript =
    @"Object.prototype.toString.call(document.forms)";

// Evaluates to the WebIDL class name of the `elements` of form `f`.
NSString* const kFormElementsClassScript =
    @"Object.prototype.toString.call(document.getElementById('f').elements)";

}  // namespace

using JavaScriptContentWorldUtilsTest = PlatformTest;

// Tests the mapping from a WKContentWorld to a web::ContentWorld enum value.
TEST_F(JavaScriptContentWorldUtilsTest, WKContentWorldToContentWorldMapping) {
  EXPECT_EQ(ContentWorld::kPageContentWorld,
            ContentWorldIdentifierForWKContentWorld(WKContentWorld.pageWorld));
  EXPECT_EQ(ContentWorld::kIsolatedWorld,
            ContentWorldIdentifierForWKContentWorld(
                WKContentWorldForContentWorldIdentifier(
                    ContentWorld::kIsolatedWorld)));
}

// Tests the mapping from a web::ContentWorld enum value to a WKContentWorld.
TEST_F(JavaScriptContentWorldUtilsTest, ContentWorldToWKContentWorldMapping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kIsolatedWorldDisableLegacyBuiltinOverrides);
  EXPECT_EQ(WKContentWorld.pageWorld, WKContentWorldForContentWorldIdentifier(
                                          ContentWorld::kPageContentWorld));
  EXPECT_EQ(
      WKContentWorld.defaultClientWorld,
      WKContentWorldForContentWorldIdentifier(ContentWorld::kIsolatedWorld));
}

using IsolatedWorldLegacyBuiltinOverridesTest = JavascriptTest;

// Tests that named elements override `document.forms` and `form.elements` in
// the isolated world when `kIsolatedWorldDisableLegacyBuiltinOverrides` is
// disabled.
TEST_F(IsolatedWorldLegacyBuiltinOverridesTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kIsolatedWorldDisableLegacyBuiltinOverrides);
  ASSERT_TRUE(LoadHtml(kClobberingHtml));

  WKContentWorld* world =
      WKContentWorldForContentWorldIdentifier(ContentWorld::kIsolatedWorld);
  EXPECT_EQ(WKContentWorld.defaultClientWorld, world);
  EXPECT_EQ(ContentWorld::kIsolatedWorld,
            ContentWorldIdentifierForWKContentWorld(world));

  EXPECT_NSEQ(
      @"[object HTMLFormElement]",
      test::ExecuteJavaScript(web_view(), world, kDocumentFormsClassScript));
  EXPECT_NSEQ(
      @"[object HTMLInputElement]",
      test::ExecuteJavaScript(web_view(), world, kFormElementsClassScript));
}

// Tests that the isolated world disables the overriding of built-in
// properties when `kIsolatedWorldDisableLegacyBuiltinOverrides` is enabled on
// iOS 27+, and falls back to `defaultClientWorld` on earlier OS versions.
TEST_F(IsolatedWorldLegacyBuiltinOverridesTest, FeatureEnabled) {
  base::test::ScopedFeatureList feature_list(
      features::kIsolatedWorldDisableLegacyBuiltinOverrides);
  ASSERT_TRUE(LoadHtml(kClobberingHtml));

  WKContentWorld* world =
      WKContentWorldForContentWorldIdentifier(ContentWorld::kIsolatedWorld);
  ASSERT_TRUE(world);
  EXPECT_EQ(world, WKContentWorldForContentWorldIdentifier(
                       ContentWorld::kIsolatedWorld));
  EXPECT_EQ(ContentWorld::kIsolatedWorld,
            ContentWorldIdentifierForWKContentWorld(world));

#if defined(__IPHONE_27_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_0
  if (@available(iOS 27, *)) {
    EXPECT_NE(WKContentWorld.defaultClientWorld, world);
    EXPECT_NSEQ(
        @"[object HTMLCollection]",
        test::ExecuteJavaScript(web_view(), world, kDocumentFormsClassScript));
    EXPECT_NSEQ(
        @"[object HTMLFormControlsCollection]",
        test::ExecuteJavaScript(web_view(), world, kFormElementsClassScript));
    return;
  }
#endif

  EXPECT_EQ(WKContentWorld.defaultClientWorld, world);
  EXPECT_NSEQ(
      @"[object HTMLFormElement]",
      test::ExecuteJavaScript(web_view(), world, kDocumentFormsClassScript));
  EXPECT_NSEQ(
      @"[object HTMLInputElement]",
      test::ExecuteJavaScript(web_view(), world, kFormElementsClassScript));
}

}  // namespace web
