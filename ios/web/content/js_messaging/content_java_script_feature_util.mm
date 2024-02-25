// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/js_messaging/content_java_script_feature_util.h"

#import "ios/web/annotations/annotations_java_script_feature.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_features/context_menu/context_menu_java_script_feature.h"
#import "ios/web/js_features/error_page/error_page_java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/text_fragments/text_fragments_java_script_feature.h"
#import "ios/web/webui/web_ui_messaging_java_script_feature.h"

namespace web {
namespace java_script_features {

std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeaturesForContent(
    BrowserState* browser_state) {
  std::vector<JavaScriptFeature*> features = {
      GetBaseJavaScriptFeature(),
      GetCommonJavaScriptFeature(),
      GetMessageJavaScriptFeature(),
      ContextMenuJavaScriptFeature::FromBrowserState(browser_state),
      ErrorPageJavaScriptFeature::GetInstance(),
      FindInPageJavaScriptFeature::GetInstance(),
      TextFragmentsJavaScriptFeature::GetInstance(),
      WebUIMessagingJavaScriptFeature::GetInstance(),
      AnnotationsJavaScriptFeature::GetInstance()};
  return features;
}

}  // namespace java_script_features
}  // namespace web
