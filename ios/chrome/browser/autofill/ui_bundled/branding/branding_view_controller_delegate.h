// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_DELEGATE_H_

// Protocol to handle user interactions in a BrandingViewController.
@protocol BrandingViewControllerDelegate

// Invoked when the user has tapped on the branding icon.
- (void)brandingIconDidPress;

// Invoked if the branding icon is visible when the keyboard pops up.
- (void)brandingIconDidShow;

// Invoked when the branding icon has performed the "pop" animation.
- (void)brandingIconDidPerformPopAnimation;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_DELEGATE_H_
