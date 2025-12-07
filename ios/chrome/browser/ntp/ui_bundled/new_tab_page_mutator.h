// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_

// Mutator for the NTP's view controller to update the mediator.
@protocol NewTabPageMutator

// The current scroll position to save in the NTP state.
@property(nonatomic, assign) CGFloat scrollPositionToSave;

// Notifies that the Lens "new" badge has been displayed.
- (void)notifyLensBadgeDisplayed;

// Notifies that the Customization "new" badge has been displayed.
- (void)notifyCustomizationBadgeDisplayed;

// Notifies the caller to check if any new badge is eligible to be shown.
- (void)checkNewBadgeEligibility;

// Notifies that the NTP has been displayed in landscape.
- (void)notifyNtpDisplayedInLandscape;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MUTATOR_H_
