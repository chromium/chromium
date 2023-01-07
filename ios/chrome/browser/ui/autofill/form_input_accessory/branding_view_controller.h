// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol to handle user interactions in a BrandingViewController.
@protocol BrandingViewControllerDelegate;

// Creates and manages a view with branding icon. This will show at the left
// side of `FormInputAccessoryViewController` when autofill branding should be
// shown.
@interface BrandingViewController : UIViewController

// Delegate to handle interactions.
@property(nonatomic, weak) id<BrandingViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_
