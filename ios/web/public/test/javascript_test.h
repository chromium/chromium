// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_JAVASCRIPT_TEST_H_
#define IOS_WEB_PUBLIC_TEST_JAVASCRIPT_TEST_H_

#import <WebKit/WebKit.h>

#import "testing/platform_test.h"
#import "url/gurl.h"

namespace web {

// A test fixture exposing a WKWebView for testing JavaScript files directly.
// This fixture intentionally does not setup standard //ios/web objects.
class JavascriptTest : public PlatformTest {
 protected:
  JavascriptTest();
  ~JavascriptTest() override;

  // Loads `html` into `web_view()` and waits for the web view to finish
  // loading.
  bool LoadHtml(NSString* html);

  // Loads `url` into `web_view()` and waits for the web view to finish
  // loading.
  bool LoadUrl(const GURL& url);

  // Adds the script which configures `__gCrWeb` to `web_view()`s
  // configuration.
  void AddGCrWebScript();
  // Adds the script which configures `__gCrWeb.common` to `web_view()`s
  // configuration.
  void AddCommonScript();
  // Adds the script which configures `__gCrWeb.message` to `web_view()`s
  // configuration.
  void AddMessageScript();

  // Adds the script with name `script_name` to `web_view()`s configuration.
  void AddUserScript(NSString* script_name);

  // Returns the WKWebView used for loading page content via `LoadHtml`.
  WKWebView* web_view() { return web_view_; }

 private:
  WKWebView* web_view_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_JAVASCRIPT_TEST_H_
