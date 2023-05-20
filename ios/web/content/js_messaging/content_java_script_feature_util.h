// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_UTIL_H_

#include <vector>

namespace web {

class BrowserState;
class JavaScriptFeature;

namespace java_script_features {

// Returns the JavaScriptFeatures built in to //ios/web, that are used in
// ios/web/content.
std::vector<JavaScriptFeature*> GetBuiltInJavaScriptFeaturesForContent(
    BrowserState* browser_state);

}  // namespace java_script_features
}  // namespace web

#endif  // IOS_WEB_CONTENT_JS_MESSAGING_CONTENT_JAVA_SCRIPT_FEATURE_UTIL_H_
