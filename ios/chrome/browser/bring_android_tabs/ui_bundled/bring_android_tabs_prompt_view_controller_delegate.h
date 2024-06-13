// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate object that handles Chromium model updates according to user action.
@protocol BringAndroidTabsPromptViewControllerDelegate

// Called when prompt is visible.
- (void)bringAndroidTabsPromptViewControllerDidShow;

// User has tapped "open all tabs" button.
- (void)bringAndroidTabsPromptViewControllerDidTapOpenAllButton;

// User has tapped "review" button.
- (void)bringAndroidTabsPromptViewControllerDidTapReviewButton;

// User has dismissed the prompt. If the dismissal is done using a modal swipe,
// the parameter `swiped` is YES.
- (void)bringAndroidTabsPromptViewControllerDidDismissWithSwipe:(BOOL)swiped
    NS_SWIFT_NAME(bringAndroidTabsPromptViewControllerDidDismiss(swiped:));

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_VIEW_CONTROLLER_DELEGATE_H_
