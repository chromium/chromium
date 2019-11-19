// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_matchers_app_interface.h"

#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#import "ios/web/shell/view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShellMatchersAppInterface

+ (id<GREYMatcher>)webView {
  return web::WebViewInWebState(web::shell_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webViewScrollView {
  return web::WebViewScrollView(web::shell_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)backButton {
  return grey_accessibilityLabel(kWebShellBackButtonAccessibilityLabel);
}

+ (id<GREYMatcher>)forwardButton {
  return grey_accessibilityLabel(kWebShellForwardButtonAccessibilityLabel);
}

@end
