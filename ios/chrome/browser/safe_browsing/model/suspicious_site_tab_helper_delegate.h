// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for `SuspiciousSiteTabHelper` to present and dismiss the suspicious
// site warning UI.
@protocol SuspiciousSiteTabHelperDelegate <NSObject>

// Instructs the delegate to present the suspicious site warning alert for the
// active WebState.
- (void)showSuspiciousSiteWarning;

// Instructs the delegate to dismiss any currently presented suspicious site
// warning alert.
- (void)dismissSuspiciousSiteWarning;

@end

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_SUSPICIOUS_SITE_TAB_HELPER_DELEGATE_H_
