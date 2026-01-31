// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A Javascript feature for testing purposes that exposes remote frame
// registration to native-side tests.
class PageContextWrapperTestJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static PageContextWrapperTestJavaScriptFeature* GetInstance();

 private:
  friend class base::NoDestructor<PageContextWrapperTestJavaScriptFeature>;

  PageContextWrapperTestJavaScriptFeature();
  ~PageContextWrapperTestJavaScriptFeature() override;

  PageContextWrapperTestJavaScriptFeature(
      const PageContextWrapperTestJavaScriptFeature&) = delete;
  PageContextWrapperTestJavaScriptFeature& operator=(
      const PageContextWrapperTestJavaScriptFeature&) = delete;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_TEST_JAVA_SCRIPT_FEATURE_H_
