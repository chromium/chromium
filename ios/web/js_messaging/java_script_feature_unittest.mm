// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_messaging/java_script_feature.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef PlatformTest JavaScriptFeatureTest;

// Tests the creation of FeatureScripts.
TEST_F(JavaScriptFeatureTest, CreateFeatureScript) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  auto feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  EXPECT_EQ(document_start_injection_time, feature_script.GetInjectionTime());
  EXPECT_EQ(target_frames_all, feature_script.GetTargetFrames());
  EXPECT_TRUE([feature_script.GetScriptString() containsString:@"__gCrWeb"]);

  auto document_end_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd;
  auto target_frames_main =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame;
  auto feature_script2 =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "common_js", document_end_injection_time, target_frames_main);

  EXPECT_EQ(document_end_injection_time, feature_script2.GetInjectionTime());
  EXPECT_EQ(target_frames_main, feature_script2.GetTargetFrames());
  EXPECT_TRUE(
      [feature_script2.GetScriptString() containsString:@"__gCrWeb.common"]);
}

// Tests creating a JavaScriptFeature.
TEST_F(JavaScriptFeatureTest, CreateFeature) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  const web::JavaScriptFeature::FeatureScript feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  auto any_content_world =
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld;
  web::JavaScriptFeature feature(any_content_world, {feature_script});

  EXPECT_EQ(any_content_world, feature.GetSupportedContentWorld());
  EXPECT_EQ(0ul, feature.GetDependentFeatures().size());

  auto feature_scripts = feature.GetScripts();
  ASSERT_EQ(1ul, feature_scripts.size());
  EXPECT_NSEQ(feature_script.GetScriptString(),
              feature_scripts[0].GetScriptString());
}

// Tests creating a JavaScriptFeature which relies on a dependent feature.
TEST_F(JavaScriptFeatureTest, CreateFeatureWithDependentFeature) {
  auto document_start_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart;
  auto target_frames_all =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;
  const web::JavaScriptFeature::FeatureScript dependent_feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "base_js", document_start_injection_time, target_frames_all);

  auto document_end_injection_time =
      web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd;
  auto target_frames_main =
      web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame;
  const web::JavaScriptFeature::FeatureScript feature_script =
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "common_js", document_end_injection_time, target_frames_main);

  auto page_content_world =
      web::JavaScriptFeature::ContentWorld::kPageContentWorld;
  web::JavaScriptFeature dependent_feature(page_content_world,
                                           {dependent_feature_script});
  web::JavaScriptFeature feature(page_content_world, {feature_script},
                                 {&dependent_feature});
  EXPECT_EQ(page_content_world, feature.GetSupportedContentWorld());

  auto feature_scripts = feature.GetScripts();
  ASSERT_EQ(1ul, feature_scripts.size());
  auto dependent_features = feature.GetDependentFeatures();
  ASSERT_EQ(1ul, dependent_features.size());
  auto dependent_feature_scripts = dependent_features[0]->GetScripts();
  ASSERT_EQ(1ul, dependent_feature_scripts.size());
  EXPECT_NSEQ(feature_script.GetScriptString(),
              feature_scripts[0].GetScriptString());
  EXPECT_NSEQ(dependent_feature_script.GetScriptString(),
              dependent_feature_scripts[0].GetScriptString());
}
