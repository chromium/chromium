// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_util_impl.h"

#import <Foundation/Foundation.h>

#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "testing/platform_test.h"

typedef PlatformTest JavaScriptFeatureUtilImplTest;

TEST_F(JavaScriptFeatureUtilImplTest, BaseFeature) {
  web::JavaScriptFeature* feature =
      web::java_script_features::GetBaseJavaScriptFeature();

  std::vector<const web::JavaScriptFeature::FeatureScript*> scripts =
      feature->GetScripts();
  EXPECT_EQ(1ul, scripts.size());

  const web::JavaScriptFeature::FeatureScript* script = scripts.front();
  EXPECT_TRUE([script->GetScriptString() containsString:@"__gCrWeb"]);
  EXPECT_FALSE([script->GetScriptString() containsString:@"__gCrWeb.common"]);

  EXPECT_EQ(
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
      script->GetInjectionTime());
  EXPECT_EQ(web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
            script->GetTargetFrames());
}

TEST_F(JavaScriptFeatureUtilImplTest, CommonFeature) {
  web::JavaScriptFeature* feature =
      web::java_script_features::GetCommonJavaScriptFeature();

  std::vector<const web::JavaScriptFeature::FeatureScript*> scripts =
      feature->GetScripts();
  EXPECT_EQ(1ul, scripts.size());

  const web::JavaScriptFeature::FeatureScript* script = scripts.front();
  EXPECT_TRUE([script->GetScriptString() containsString:@"__gCrWeb.common"]);
  EXPECT_FALSE([script->GetScriptString() containsString:@"__gCrWeb.message"]);

  EXPECT_EQ(
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
      script->GetInjectionTime());
  EXPECT_EQ(web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
            script->GetTargetFrames());
}

TEST_F(JavaScriptFeatureUtilImplTest, MessageFeature) {
  web::JavaScriptFeature* feature =
      web::java_script_features::GetMessageJavaScriptFeature();

  std::vector<const web::JavaScriptFeature::FeatureScript*> scripts =
      feature->GetScripts();
  EXPECT_EQ(1ul, scripts.size());

  const web::JavaScriptFeature::FeatureScript* script = scripts.front();
  EXPECT_TRUE([script->GetScriptString() containsString:@"__gCrWeb.message"]);
  EXPECT_EQ(
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
      script->GetInjectionTime());
  EXPECT_EQ(web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
            script->GetTargetFrames());
}

// Tests that the built in features are returned as expected.
TEST_F(JavaScriptFeatureUtilImplTest, BuiltInFeatures) {
  std::vector features =
      web::java_script_features::GetBuiltInJavaScriptFeatures();
  ASSERT_EQ(1ul, features.size());

  web::JavaScriptFeature* feature = features.front();
  EXPECT_EQ(web::java_script_features::GetContextMenuJavaScriptFeature(),
            feature);
}
