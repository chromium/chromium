// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/fake_cwv_navigation_delegate.h"

@interface FakeCWVNavigationDelegate ()
@property(nonatomic, readwrite) BOOL navigationFinished;
@end

@implementation FakeCWVNavigationDelegate

#pragma mark - CWVNavigationDelegate

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  self.navigationFinished = YES;
}

@end
