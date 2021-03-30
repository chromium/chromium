// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_util_impl.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/favicon/favicon_java_script_feature.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#include "ios/web/js_features/context_menu/context_menu_java_script_feature.h"
#include "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"
#import "ios/web/js_features/window_error/window_error_java_script_feature.h"
#include "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kBaseScriptName[] = "base_js";
const char kCommonScriptName[] = "common_js";
const char kMessageScriptName[] = "message_js";
const char kPluginPlaceholderScriptName[] = "plugin_placeholder_js";

const char kMainFrameDescription[] = "Main frame";
const char kIframeDescription[] = "Iframe";

// Returns a string with \ and ' escaped.
// This is used instead of GetQuotedJSONString because that will convert
// UTF-16 to UTF-8, which can cause problems when injecting scripts depending
// on the page encoding (see crbug.com/302741).
NSString* EscapedQuotedString(NSString* string) {
  string = [string stringByReplacingOccurrencesOfString:@"\\"
                                             withString:@"\\\\"];
  return [string stringByReplacingOccurrencesOfString:@"'" withString:@"\\'"];
}
static dispatch_once_t get_plugin_placeholder_once;

web::FaviconJavaScriptFeature* GetFaviconJavaScriptFeature() {
  // Static storage is ok for |favicon_feature| as it holds no state.
  static std::unique_ptr<web::FaviconJavaScriptFeature> favicon_feature =
      nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    favicon_feature = std::make_unique<web::FaviconJavaScriptFeature>();
  });
  return favicon_feature.get();
}

web::WindowErrorJavaScriptFeature* GetWindowErrorJavaScriptFeature() {
  // Static storage is ok for |window_error_feature| as it holds no state.
  static std::unique_ptr<web::WindowErrorJavaScriptFeature>
      window_error_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    window_error_feature =
        std::make_unique<web::WindowErrorJavaScriptFeature>(base::BindRepeating(
            ^(web::WindowErrorJavaScriptFeature::ErrorDetails error_details) {
              // Displays the JavaScript error details in the following format:
              //   _________ JavaScript error: _________
              //     {error_message}
              //     {url} | {filename}:{line_number}
              //     {kMainFrameDescription|kIframeDescription}
              const char* frame_description = error_details.is_main_frame
                                                  ? kMainFrameDescription
                                                  : kIframeDescription;
              DLOG(ERROR) << "\n_________ JavaScript error: _________"
                          << "\n  "
                          << base::SysNSStringToUTF8(error_details.message)
                          << "\n  " << error_details.url.spec() << " | "
                          << base::SysNSStringToUTF8(error_details.filename)
                          << ":" << error_details.line_number << "\n  "
                          << frame_description;
            }));
  });
  return window_error_feature.get();
}

web::JavaScriptFeature* GetPluginPlaceholderJavaScriptFeature() {
  // Static storage is ok for |plugin_placeholder_feature| as it holds no state.
  static std::unique_ptr<web::JavaScriptFeature> plugin_placeholder_feature =
      nullptr;
  dispatch_once(&get_plugin_placeholder_once, ^{
    std::map<std::string, NSString*> replacement_map{
        {"$(PLUGIN_NOT_SUPPORTED_TEXT)",
         EscapedQuotedString(base::SysUTF16ToNSString(
             web::GetWebClient()->GetPluginNotSupportedText()))}};
    std::vector<const web::JavaScriptFeature::FeatureScript> feature_scripts = {
        web::JavaScriptFeature::FeatureScript::CreateWithFilename(
            kPluginPlaceholderScriptName,
            web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
            web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
            web::JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                kReinjectOnDocumentRecreation,
            replacement_map)};

    plugin_placeholder_feature = std::make_unique<web::JavaScriptFeature>(
        web::JavaScriptFeature::ContentWorld::kAnyContentWorld,
        feature_scripts);
  });
  return plugin_placeholder_feature.get();
}

}  // namespace

namespace web {
namespace java_script_features {

std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeatures(
    BrowserState* browser_state) {
  return {ContextMenuJavaScriptFeature::FromBrowserState(browser_state),
          FindInPageJavaScriptFeature::GetInstance(),
          GetFaviconJavaScriptFeature(),
          GetPluginPlaceholderJavaScriptFeature(),
          GetScrollHelperJavaScriptFeature(),
          GetWindowErrorJavaScriptFeature()};
}

ScrollHelperJavaScriptFeature* GetScrollHelperJavaScriptFeature() {
  // Static storage is ok for |scroll_helper_feature| as it holds no state.
  static std::unique_ptr<ScrollHelperJavaScriptFeature> scroll_helper_feature =
      nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    scroll_helper_feature = std::make_unique<ScrollHelperJavaScriptFeature>();
  });
  return scroll_helper_feature.get();
}

JavaScriptFeature* GetBaseJavaScriptFeature() {
  // Static storage is ok for |base_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> base_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kBaseScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    base_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts);
  });
  return base_feature.get();
}

JavaScriptFeature* GetCommonJavaScriptFeature() {
  // Static storage is ok for |common_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> common_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kCommonScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    std::vector<const JavaScriptFeature*> dependencies = {
        GetBaseJavaScriptFeature()};

    common_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts,
        dependencies);
  });
  return common_feature.get();
}

JavaScriptFeature* GetMessageJavaScriptFeature() {
  // Static storage is ok for |message_feature| as it holds no state.
  static std::unique_ptr<JavaScriptFeature> message_feature = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    std::vector<const JavaScriptFeature::FeatureScript> feature_scripts = {
        JavaScriptFeature::FeatureScript::CreateWithFilename(
            kMessageScriptName,
            JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
            JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

    std::vector<const JavaScriptFeature*> dependencies = {
        GetCommonJavaScriptFeature()};

    message_feature = std::make_unique<JavaScriptFeature>(
        JavaScriptFeature::ContentWorld::kAnyContentWorld, feature_scripts,
        dependencies);
  });
  return message_feature.get();
}

void ResetPluginPlaceholderJavaScriptFeature() {
  get_plugin_placeholder_once = 0;
}

}  // namespace java_script_features
}  // namespace web
