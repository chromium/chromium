// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_IMPL_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_IMPL_H_

#include <vector>

#include "ios/web/public/js_messaging/java_script_feature_util.h"

namespace web {

class BrowserState;
class JavaScriptFeature;
class ScrollHelperJavaScriptFeature;

namespace java_script_features {

// Returns the JavaScriptFeatures built in to //ios/web.
std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeatures(
    BrowserState* browser_state);

// Returns the ScrollHelperJavaScriptFeature.
ScrollHelperJavaScriptFeature* GetScrollHelperJavaScriptFeature();

}  // namespace java_script_features
}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_IMPL_H_
