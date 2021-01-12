// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_content_world.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

WKUserScriptInjectionTime InjectionTimeToWKUserScriptInjectionTime(
    JavaScriptFeature::FeatureScript::InjectionTime injection_time) {
  switch (injection_time) {
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart:
      return WKUserScriptInjectionTimeAtDocumentStart;
    case JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd:
      return WKUserScriptInjectionTimeAtDocumentEnd;
  }
  NOTREACHED();
  return WKUserScriptInjectionTimeAtDocumentStart;
}

}  // namespace

JavaScriptContentWorld::JavaScriptContentWorld(
    WKUserContentController* user_content_controller)
    : user_content_controller_(user_content_controller) {}

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
JavaScriptContentWorld::JavaScriptContentWorld(
    WKUserContentController* user_content_controller,
    WKContentWorld* content_world)
    : user_content_controller_(user_content_controller),
      content_world_(content_world) {}
#endif  // defined(__IPHONE14_0)

JavaScriptContentWorld::~JavaScriptContentWorld() {}

bool JavaScriptContentWorld::HasFeature(const JavaScriptFeature* feature) {
  return features_.find(feature) != features_.end();
}

void JavaScriptContentWorld::AddFeature(const JavaScriptFeature* feature) {
  if (HasFeature(feature)) {
    // |feature| has already been added to this content world.
    return;
  }

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    // Ensure |feature| supports this content world.
    if (content_world_ && content_world_ != WKContentWorld.pageWorld) {
      DCHECK_EQ(feature->GetSupportedContentWorld(),
                JavaScriptFeature::ContentWorld::kAnyContentWorld);
    }
  }
#endif  // defined(__IPHONE14_0)

  features_.insert(feature);

  // Add dependent features first.
  for (const JavaScriptFeature* dep_feature : feature->GetDependentFeatures()) {
    AddFeature(dep_feature);
  }

  // Setup user scripts.
  for (const JavaScriptFeature::FeatureScript& feature_script :
       feature->GetScripts()) {
    WKUserScriptInjectionTime injection_time =
        InjectionTimeToWKUserScriptInjectionTime(
            feature_script.GetInjectionTime());

    bool main_frame_only =
        feature_script.GetTargetFrames() !=
        JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;

    WKUserScript* user_script = nil;
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
    if (@available(iOS 14, *)) {
      if (content_world_) {
        user_script = [[WKUserScript alloc]
              initWithSource:feature_script.GetScriptString()
               injectionTime:injection_time
            forMainFrameOnly:main_frame_only
              inContentWorld:content_world_];
      }
    }
#endif  // defined(__IPHONE14_0)

    if (!user_script) {
      user_script =
          [[WKUserScript alloc] initWithSource:feature_script.GetScriptString()
                                 injectionTime:injection_time
                              forMainFrameOnly:main_frame_only];
    }

    [user_content_controller_ addUserScript:user_script];
  }
}

}  // namespace web
