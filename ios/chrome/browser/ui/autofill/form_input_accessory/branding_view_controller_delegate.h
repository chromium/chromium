// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_DELEGATE_H_

// Protocol to handle user interactions in a BrandingViewController.
@protocol BrandingViewControllerDelegate

// Invoked when the user has tapped on the branding icon.
- (void)brandingIconPressed;

// Whether the branding icon should perform animation; should be checked
// every time the branding icon shows.
- (BOOL)brandingIconShouldPerformPopAnimation;

// Invoked when the branding icon has performed the "pop" animation.
- (void)brandingIconDidPerformPopAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_DELEGATE_H_
