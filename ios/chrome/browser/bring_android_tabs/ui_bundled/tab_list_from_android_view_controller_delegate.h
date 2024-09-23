// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_DELEGATE_H_

// Delegate protocol that handles model updates on interaction with the "Tab
// List From Android" table view controller.
@protocol TabListFromAndroidViewControllerDelegate

// Called when the user dismisses the prompt. If the dismissal is done using a
// modal swipe, the parameter `swiped` is YES. `countDeselected` is the number
// of tabs the user deselected before dismissal.
- (void)tabListFromAndroidViewControllerDidDismissWithSwipe:(BOOL)swiped
                                     numberOfDeselectedTabs:
                                         (int)countDeselected;

// Called when the user taps the "open" button. Values in `tabIndices`
// correspond to the indices of the tabs the user wants to open in the
// BringAndroidTabsToIOSService.
- (void)tabListFromAndroidViewControllerDidTapOpenButtonWithTabIndices:
    (NSArray<NSNumber*>*)tabIndices;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_TAB_LIST_FROM_ANDROID_VIEW_CONTROLLER_DELEGATE_H_
