// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#import "base/ios/ios_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

// A test fixture for testing JavaScriptFeatureManager.
class JavaScriptFeatureManagerTest : public web::WebTest {
 protected:
  JavaScriptFeatureManagerTest()
      : web::WebTest(std::make_unique<web::FakeWebClient>()) {}

  web::JavaScriptFeatureManager* GetJavaScriptFeatureManager() {
    web::JavaScriptFeatureManager* java_script_feature_manager =
        web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState());
    return java_script_feature_manager;
  }

  WKUserContentController* GetUserContentController() {
    return web::WKWebViewConfigurationProvider::FromBrowserState(
               GetBrowserState())
        .GetWebViewConfiguration()
        .userContentController;
  }

  void SetUp() override {
    web::WebTest::SetUp();
    [GetUserContentController() removeAllUserScripts];
  }
};

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document start time for the page content world.
TEST_F(JavaScriptFeatureManagerTest, AllFramesStartFeature) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());

  const std::vector<web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::ContentWorld::kPageContentWorld, feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  EXPECT_EQ(1ul, [GetUserContentController().userScripts count]);
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE([user_script.source containsString:@"javaScriptFeatureTest="]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentStart,
            user_script.injectionTime);
  EXPECT_EQ(NO, user_script.forMainFrameOnly);
}

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document end time for the page content world.
TEST_F(JavaScriptFeatureManagerTest, MainFrameEndFeature) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());

  const std::vector<web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::ContentWorld::kPageContentWorld, feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  EXPECT_EQ(1ul, [GetUserContentController().userScripts count]);
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE([user_script.source containsString:@"javaScriptFeatureTest="]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentEnd, user_script.injectionTime);
  EXPECT_EQ(YES, user_script.forMainFrameOnly);
}

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document end time for an isolated world.
TEST_F(JavaScriptFeatureManagerTest, MainFrameEndFeatureIsolatedWorld) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());

  const std::vector<web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::ContentWorld::kIsolatedWorld, feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  EXPECT_EQ(1ul, [GetUserContentController().userScripts count]);
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE([user_script.source containsString:@"javaScriptFeatureTest"]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentEnd, user_script.injectionTime);
  EXPECT_EQ(YES, user_script.forMainFrameOnly);
}
