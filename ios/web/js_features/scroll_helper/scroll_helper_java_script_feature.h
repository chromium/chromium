// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_SCROLL_HELPER_SCROLL_HELPER_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_JS_FEATURES_SCROLL_HELPER_SCROLL_HELPER_JAVA_SCRIPT_FEATURE_H_

#include "ios/web/public/js_messaging/java_script_feature.h"

@class WKScriptMessage;

namespace web {

class WebState;

// A feature which includes helpers to workaround scroll problems in WKWebView.
class ScrollHelperJavaScriptFeature : public JavaScriptFeature {
 public:
  ~ScrollHelperJavaScriptFeature() override;
  ScrollHelperJavaScriptFeature();

  ScrollHelperJavaScriptFeature(const ScrollHelperJavaScriptFeature&) = delete;
  ScrollHelperJavaScriptFeature& operator=(
      const ScrollHelperJavaScriptFeature&) = delete;

  // Sets the scroll dragging state of the page to `dragging`. window.scrollTo
  // is overridden to be suppressed as long as the `dragging` is true.
  void SetWebViewScrollViewIsDragging(WebState* web_state, bool dragging);
};

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_SCROLL_HELPER_SCROLL_HELPER_JAVA_SCRIPT_FEATURE_H_
