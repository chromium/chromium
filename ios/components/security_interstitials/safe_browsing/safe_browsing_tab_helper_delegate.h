// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_DELEGATE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_DELEGATE_H_

// Delegate for SafeBrowsingTabHelper.
@protocol SafeBrowsingTabHelperDelegate <NSObject>

// Sends call to open safe browsing settings menu.
- (void)openSafeBrowsingSettings;

// Sends call to open Enhanced Safe Browsing infobar.
- (void)showEnhancedSafeBrowsingInfobar;

@end

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_DELEGATE_H_
