// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_util_impl.h"

#import <Foundation/Foundation.h>

#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/annotations/annotations_java_script_feature.h"
#import "ios/web/common/features.h"
#import "ios/web/favicon/favicon_java_script_feature.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"
#import "ios/web/js_features/error_page/error_page_java_script_feature.h"
#import "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"
#import "ios/web/js_features/window_error/window_error_java_script_feature.h"
#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"
#import "ios/web/navigation/navigation_java_script_feature.h"
#import "ios/web/navigation/session_restore_java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/web_client.h"
#import "ios/web/text_fragments/text_fragments_java_script_feature.h"
#import "ios/web/webui/web_ui_messaging_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

const char kBaseScriptName[] = "gcrweb";
const char kCommonScriptName[] = "common";
const char kMessageScriptName[] = "message";
const char kPluginPlaceholderScriptName[] = "plugin_placeholder";
const char kShareWorkaroundScriptName[] = "share_workaround";

const char kMainFrameDescription[] = "Main frame";
const char kIframeDescription[] = "Iframe";

// Returns the dictionary for placeholder replacements.
NSDictionary<NSString*, NSString*>* PlaceholderReplacements() {
  // The replacement value is computed dynamically each time this function is
  // evaluated as the WebClient may change (in case of tests) or the returned
  // value may change over time (nothing prevent a WebClient from doing that).
  NSString* replacement =
      base::SysUTF16ToNSString(GetWebClient()->GetPluginNotSupportedText());

  // Escape the \ and ' characters in replacement. This is not done using the
  // GetQuotedJSONString() function as it converts UTF-16 to UTF-8 which can
  // cause problems when injecting script depending on the page enconding.
  // See https://crbug.com/302741/.
  replacement = [replacement stringByReplacingOccurrencesOfString:@"\\"
                                                       withString:@"\\\\"];
  replacement = [replacement stringByReplacingOccurrencesOfString:@"'"
                                                       withString:@"\'"];

  return @{@"$(PLUGIN_NOT_SUPPORTED_TEXT)" : replacement};
}

FaviconJavaScriptFeature* GetFaviconJavaScriptFeature() {
  // Static storage is ok for `favicon_feature` as it holds no state.
  static base::NoDestructor<FaviconJavaScriptFeature> favicon_feature;
  return favicon_feature.get();
}

WindowErrorJavaScriptFeature* GetWindowErrorJavaScriptFeature() {
  // Static storage is ok for `window_error_feature` as it holds no state.
  static base::NoDestructor<WindowErrorJavaScriptFeature> window_error_feature(
      base::BindRepeating(^(
          WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        // Displays the JavaScript error details in the following format:
        //   _________ JavaScript error: _________
        //     {error_message}
        //     {url} | {filename}:{line_number}
        //     {kMainFrameDescription|kIframeDescription}
        const char* frame_description = error_details.is_main_frame
                                            ? kMainFrameDescription
                                            : kIframeDescription;
        DLOG(ERROR) << "\n_________ JavaScript error: _________"
                    << "\n  " << base::SysNSStringToUTF8(error_details.message)
                    << "\n  " << error_details.url.spec() << " | "
                    << base::SysNSStringToUTF8(error_details.filename) << ":"
                    << error_details.line_number << "\n  " << frame_description;
      }));
  return window_error_feature.get();
}

JavaScriptFeature* GetPluginPlaceholderJavaScriptFeature() {
  // Static storage is ok for `plugin_placeholder_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> plugin_placeholder_feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld,
      std::vector<const JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kPluginPlaceholderScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
              JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                  kReinjectOnDocumentRecreation,
              base::BindRepeating(&PlaceholderReplacements))}));
  return plugin_placeholder_feature.get();
}

JavaScriptFeature* GetShareWorkaroundJavaScriptFeature() {
  // Static storage is ok for `share_workaround_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> share_workaround_feature(
      JavaScriptFeature::ContentWorld::kPageContentWorld,
      std::vector<const JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kShareWorkaroundScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames,
              JavaScriptFeature::FeatureScript::ReinjectionBehavior::
                  kInjectOncePerWindow)}));
  return share_workaround_feature.get();
}

}  // namespace

namespace java_script_features {

std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeatures(
    BrowserState* browser_state) {
  std::vector<JavaScriptFeature*> features = {
      ContextMenuJavaScriptFeature::FromBrowserState(browser_state),
      ErrorPageJavaScriptFeature::GetInstance(),
      FindInPageJavaScriptFeature::GetInstance(),
      GetFaviconJavaScriptFeature(),
      GetScrollHelperJavaScriptFeature(),
      GetShareWorkaroundJavaScriptFeature(),
      GetWindowErrorJavaScriptFeature(),
      NavigationJavaScriptFeature::GetInstance(),
      SessionRestoreJavaScriptFeature::FromBrowserState(browser_state),
      TextFragmentsJavaScriptFeature::GetInstance(),
      WebFramesManagerJavaScriptFeature::FromBrowserState(browser_state),
      WebUIMessagingJavaScriptFeature::GetInstance()};

  // Plugin Placeholder is no longer used as of iOS 14.5 as <applet> support is
  // completely removed.
  // TODO(crbug.com/1218221): Remove feature once app is iOS 14.5+.
  if (!base::ios::IsRunningOnOrLater(14, 5, 0)) {
    features.push_back(GetPluginPlaceholderJavaScriptFeature());
  }

  if (base::FeatureList::IsEnabled(web::features::kEnableWebPageAnnotations)) {
    features.push_back(AnnotationsJavaScriptFeature::GetInstance());
  }

  return features;
}

ScrollHelperJavaScriptFeature* GetScrollHelperJavaScriptFeature() {
  // Static storage is ok for `scroll_helper_feature` as it holds no state.
  static base::NoDestructor<ScrollHelperJavaScriptFeature>
      scroll_helper_feature;
  return scroll_helper_feature.get();
}

JavaScriptFeature* GetBaseJavaScriptFeature() {
  // Static storage is ok for `base_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> base_feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld,
      std::vector<const JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kBaseScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}));
  return base_feature.get();
}

JavaScriptFeature* GetCommonJavaScriptFeature() {
  // Static storage is ok for `common_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> common_feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld,
      std::vector<const JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kCommonScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}),
      std::vector<const JavaScriptFeature*>({GetBaseJavaScriptFeature()}));
  return common_feature.get();
}

JavaScriptFeature* GetMessageJavaScriptFeature() {
  // Static storage is ok for `message_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> message_feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld,
      std::vector<const JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kMessageScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}),
      std::vector<const JavaScriptFeature*>({GetCommonJavaScriptFeature()}));
  return message_feature.get();
}

}  // namespace java_script_features
}  // namespace web
