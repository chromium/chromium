// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_H_

namespace web {

class JavaScriptFeature;

namespace java_script_features {

// Returns the shared base javascript used across many features which defines
// the __gCrWeb object.
JavaScriptFeature* GetBaseJavaScriptFeature();

// Returns the shared common javascript used across many features which defines
// __gCrWeb.common APIs.
JavaScriptFeature* GetCommonJavaScriptFeature();

// Returns the shared message javascript used across many features which defines
// __gCrWeb.message APIs.
JavaScriptFeature* GetMessageJavaScriptFeature();

}  // namespace java_script_features
}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_JAVA_SCRIPT_FEATURE_UTIL_H_
