// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_FAKE_CWV_NAVIGATION_DELEGATE_H_
#define IOS_WEB_VIEW_TEST_FAKE_CWV_NAVIGATION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/web_view/public/cwv_navigation_delegate.h"

NS_ASSUME_NONNULL_BEGIN

// Fake implementation of `CWVNavigationDelegate` used to observe navigation
// events in tests without intercepting optional policy-decision methods.
@interface FakeCWVNavigationDelegate : NSObject <CWVNavigationDelegate>

// Whether `-webViewDidFinishNavigation:` has been called.
@property(nonatomic, readonly) BOOL navigationFinished;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_TEST_FAKE_CWV_NAVIGATION_DELEGATE_H_
