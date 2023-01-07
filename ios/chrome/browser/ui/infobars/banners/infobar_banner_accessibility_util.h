// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_ACCESSIBILITY_UTIL_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_ACCESSIBILITY_UTIL_H_

#import <UIKit/UIKit.h>

// Updates the accessibility of the presenting view controller so that VoiceOver
// users have the ability to select other elements while the banner is
// presented.  This should be called after the banner's presentation is
// finished.  `presenting_view_controller` and `banner_view` must not be nil.
void UpdateBannerAccessibilityForPresentation(
    UIViewController* presenting_view_controller,
    UIView* banner_view);

// Removes the banner view from `presenting_view_controller`'s accessibility
// elements.  This should be called after the banner's dismissal is finished.
// `presenting_view_controller` must not be nil.
void UpdateBannerAccessibilityForDismissal(
    UIViewController* presenting_view_controller);

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_ACCESSIBILITY_UTIL_H_
