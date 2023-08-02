// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_matchers_app_interface.h"

namespace web {

id<GREYMatcher> WebView() {
  return [ShellMatchersAppInterface webView];
}

id<GREYMatcher> WebViewScrollView() {
  return [ShellMatchersAppInterface webViewScrollView];
}

id<GREYMatcher> BackButton() {
  return [ShellMatchersAppInterface backButton];
}

id<GREYMatcher> ForwardButton() {
  return [ShellMatchersAppInterface forwardButton];
}

}  // namespace web
