// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_java_script_feature_manager.h"

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/js_injection/browser/js_communication_host.h"
#import "content/public/browser/render_frame_host.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

std::u16string MakeInjectableIntoMainFrameOnly(const std::u16string& script) {
  std::u16string format_string = u"if (window == window.top) { $1 }";
  return base::ReplaceStringPlaceholders(format_string, script,
                                         /*offset=*/nullptr);
}

}  // namespace

ContentJavaScriptFeatureManager::ContentJavaScriptFeatureManager(
    std::vector<JavaScriptFeature*> features) {
  for (JavaScriptFeature* feature : features) {
    AddFeature(feature);
  }
}

ContentJavaScriptFeatureManager::~ContentJavaScriptFeatureManager() {}

void ContentJavaScriptFeatureManager::AddDocumentStartScripts(
    js_injection::JsCommunicationHost* js_communication_host) {
  for (std::u16string user_script : document_start_scripts_) {
    js_communication_host->AddDocumentStartJavaScript(user_script, {"*"});
  }
}

void ContentJavaScriptFeatureManager::InjectDocumentEndScripts(
    content::RenderFrameHost* render_frame_host) {
  for (std::u16string user_script : document_end_scripts_) {
    render_frame_host->ExecuteJavaScript(
        user_script, content::RenderFrameHost::JavaScriptResultCallback());
  }
}

bool ContentJavaScriptFeatureManager::HasFeature(
    const JavaScriptFeature* feature) const {
  return features_.find(feature) != features_.end();
}

void ContentJavaScriptFeatureManager::AddFeature(
    const JavaScriptFeature* feature) {
  if (HasFeature(feature)) {
    return;
  }

  features_.insert(feature);

  // Add dependent features.
  for (const JavaScriptFeature* dep_feature : feature->GetDependentFeatures()) {
    AddFeature(dep_feature);
  }

  // Setup user scripts.
  for (const JavaScriptFeature::FeatureScript& feature_script :
       feature->GetScripts()) {
    std::u16string user_script =
        base::SysNSStringToUTF16(feature_script.GetScriptString());
    bool main_frame_only =
        feature_script.GetTargetFrames() !=
        JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames;

    if (main_frame_only) {
      user_script = MakeInjectableIntoMainFrameOnly(user_script);
    }

    if (feature_script.GetInjectionTime() ==
        JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart) {
      document_start_scripts_.push_back(user_script);
    } else {
      document_end_scripts_.push_back(user_script);
    }
  }

  // TODO(crbug.com/1423527): Add mapping for script message handlers.
}

}  // namespace web
