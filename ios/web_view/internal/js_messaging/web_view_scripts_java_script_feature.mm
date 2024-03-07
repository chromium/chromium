// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/js_messaging/web_view_scripts_java_script_feature.h"

#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace {

const char kWebViewScriptsJavaScriptFeatureKeyName[] =
    "web_view_scripts_java_script_feature";

}  // namespace

WebViewScriptsJavaScriptFeature::WebViewScriptsJavaScriptFeature(
    web::BrowserState* browser_state)
    : web::JavaScriptFeature(web::ContentWorld::kPageContentWorld,
                             /*feature_scripts=*/{}),
      browser_state_(browser_state) {}
WebViewScriptsJavaScriptFeature::~WebViewScriptsJavaScriptFeature() = default;

// static
WebViewScriptsJavaScriptFeature*
WebViewScriptsJavaScriptFeature::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK(browser_state);

  WebViewScriptsJavaScriptFeature* feature =
      static_cast<WebViewScriptsJavaScriptFeature*>(
          browser_state->GetUserData(kWebViewScriptsJavaScriptFeatureKeyName));
  if (!feature) {
    feature = new WebViewScriptsJavaScriptFeature(browser_state);
    browser_state->SetUserData(kWebViewScriptsJavaScriptFeatureKeyName,
                               base::WrapUnique(feature));
  }
  return feature;
}

void WebViewScriptsJavaScriptFeature::SetScripts(
    std::optional<std::string> all_frames_script,
    std::optional<std::string> main_frame_script) {
  all_frames_script_ = all_frames_script;
  main_frame_script_ = main_frame_script;

  // Feature scripts must be explicitly updated after they change.
  web::WKWebViewConfigurationProvider& config_provider =
      web::WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  config_provider.UpdateScripts();
}

std::vector<WebViewScriptsJavaScriptFeature::FeatureScript>
WebViewScriptsJavaScriptFeature::GetScripts() const {
  std::vector<WebViewScriptsJavaScriptFeature::FeatureScript> feature_scripts;
  if (all_frames_script_) {
    feature_scripts.push_back(
        JavaScriptFeature::FeatureScript::CreateWithString(
            all_frames_script_.value(),
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames));
  }
  if (main_frame_script_) {
    feature_scripts.push_back(
        JavaScriptFeature::FeatureScript::CreateWithString(
            main_frame_script_.value(),
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame));
  }
  return feature_scripts;
}
