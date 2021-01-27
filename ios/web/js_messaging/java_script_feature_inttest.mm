// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/ios/wait_util.h"

#include "base/bind.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static NSString* kPageHTML =
    @"<html><body>"
     "  <div id=\"div\">contents1</div><div id=\"div2\">contents2</div>"
     "</body></html>";

namespace web {

namespace {

// Filename of the Javascript injected by FakeJavaScriptFeature which creates
// a text node on document load with the text
// |kJavaScriptFeatureTestScriptLoadedText| and exposes
// the function |kJavaScriptFeatureTestScriptReplaceDivContents|.
const char kJavaScriptFeatureTestScript[] = "java_script_feature_test_js";

// The text added to the page by |kJavaScriptFeatureTestScript| on document
// load.
const char kJavaScriptFeatureTestScriptLoadedText[] = "injected_script_loaded";

// The function exposed by kJavaScriptFeatureTestScript which replaces the
// contents of the div with |id="div"| with the text "updated".
const char kJavaScriptFeatureTestScriptReplaceDivContents[] =
    "javaScriptFeatureTest.replaceDivContents";

// A JavaScriptFeature which exposes a function to call the
// |__gCrWeb.replaceDiv1Contents| JavaScript function.
class FakeJavaScriptFeature : public JavaScriptFeature {
 public:
  FakeJavaScriptFeature(JavaScriptFeature::ContentWorld content_world)
      : JavaScriptFeature(content_world,
                          {FeatureScript::CreateWithFilename(
                              kJavaScriptFeatureTestScript,
                              FeatureScript::InjectionTime::kDocumentEnd,
                              FeatureScript::TargetFrames::kMainFrame)},
                          {}) {}
  ~FakeJavaScriptFeature() override = default;

  void ReplaceDivContents(WebFrame* web_frame) {
    CallJavaScriptFunction(web_frame,
                           kJavaScriptFeatureTestScriptReplaceDivContents, {});
  }
};

}  // namespace

typedef WebTestWithWebState JavaScriptFeatureTest;

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in the page content world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureInjectJavaScript) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kJavaScriptFeatureTestScriptLoadedText));
}

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in an isolated world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureInjectJavaScriptIsolatedWorld) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kJavaScriptFeatureTestScriptLoadedText));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in the page content world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureExecuteJavaScript) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature.ReplaceDivContents(GetMainFrame(web_state()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in an isolated world.
TEST_F(JavaScriptFeatureTest,
       JavaScriptFeatureExecuteJavaScriptInIsolatedWorld) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature.ReplaceDivContents(GetMainFrame(web_state()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

}  // namespace web
