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
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/favicon/favicon_java_script_feature.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"
#import "ios/web/js_features/error_page/error_page_java_script_feature.h"
#import "ios/web/js_features/fullscreen/fullscreen_java_script_feature.h"
#import "ios/web/js_features/scroll_helper/scroll_helper_java_script_feature.h"
#import "ios/web/js_features/window_error/error_event_listener_java_script_feature.h"
#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"
#import "ios/web/js_messaging/web_frames_manager_java_script_feature.h"
#import "ios/web/navigation/navigation_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/web_client.h"
#import "ios/web/text_fragments/text_fragments_java_script_feature.h"
#import "ios/web/webui/web_ui_messaging_java_script_feature.h"

namespace web {
namespace {

const char kBaseScriptName[] = "gcrweb";
const char kCommonScriptName[] = "common";
const char kMessageScriptName[] = "message";

const char kMainFrameDescription[] = "Main frame";
const char kIframeDescription[] = "Iframe";

FaviconJavaScriptFeature* GetFaviconJavaScriptFeature() {
  // Static storage is ok for `favicon_feature` as it holds no state.
  static base::NoDestructor<FaviconJavaScriptFeature> favicon_feature;
  return favicon_feature.get();
}

ScriptErrorMessageHandlerJavaScriptFeature*
GetScriptErrorMessageHandlerJavaScriptFeature() {
  // Static storage is ok for `window_error_feature` as it holds no state.
  static base::NoDestructor<ScriptErrorMessageHandlerJavaScriptFeature>
      script_error_message_handler_feature(base::BindRepeating(^(
          ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails
              error_details) {
        // Displays the JavaScript error details in the following format:
        //   _________ JavaScript error: _________
        //     {error_message}
        //     {url} | {filename}:{line_number}
        //     {kMainFrameDescription|kIframeDescription}
        const char* frame_description = error_details.is_main_frame
                                            ? kMainFrameDescription
                                            : kIframeDescription;
        DLOG(ERROR) << "\n_________ JavaScript error: _________" << "\n  "
                    << base::SysNSStringToUTF8(error_details.message) << "\n"
                    << base::SysNSStringToUTF8(error_details.stack) << "\n  "
                    << error_details.url.spec() << " | "
                    << base::SysNSStringToUTF8(error_details.filename) << ":"
                    << error_details.line_number << "\n  " << frame_description;
      }));
  return script_error_message_handler_feature.get();
}

}  // namespace

namespace java_script_features {

std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeatures(
    BrowserState* browser_state) {
  std::vector<JavaScriptFeature*> features = {
      GetBaseJavaScriptFeature(),
      GetCommonJavaScriptFeature(),
      GetMessageJavaScriptFeature(),
      ContextMenuJavaScriptFeature::FromBrowserState(browser_state),
      ErrorPageJavaScriptFeature::GetInstance(),
      FindInPageJavaScriptFeature::GetInstance(),
      FullscreenJavaScriptFeature::GetInstance(),
      GetFaviconJavaScriptFeature(),
      GetScrollHelperJavaScriptFeature(),
      ErrorEventListenerJavaScriptFeature::GetInstance(),
      GetScriptErrorMessageHandlerJavaScriptFeature(),
      NavigationJavaScriptFeature::GetInstance(),
      TextFragmentsJavaScriptFeature::GetInstance(),
      WebUIMessagingJavaScriptFeature::GetInstance(),
      AnnotationsJavaScriptFeature::GetInstance()};

  auto frames_manager_features = WebFramesManagerJavaScriptFeature::
      AllContentWorldFeaturesFromBrowserState(browser_state);
  features.insert(features.end(), frames_manager_features.begin(),
                  frames_manager_features.end());

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
      ContentWorld::kAllContentWorlds,
      std::vector<JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kBaseScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}));
  return base_feature.get();
}

JavaScriptFeature* GetCommonJavaScriptFeature() {
  // Static storage is ok for `common_feature` as it holds no state.
  static base::NoDestructor<JavaScriptFeature> common_feature(
      ContentWorld::kAllContentWorlds,
      std::vector<JavaScriptFeature::FeatureScript>(
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
      ContentWorld::kAllContentWorlds,
      std::vector<JavaScriptFeature::FeatureScript>(
          {JavaScriptFeature::FeatureScript::CreateWithFilename(
              kMessageScriptName,
              JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
              JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)}),
      std::vector<const JavaScriptFeature*>({GetCommonJavaScriptFeature()}));
  return message_feature.get();
}

}  // namespace java_script_features
}  // namespace web
