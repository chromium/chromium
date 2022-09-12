// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol to handle user interactions in a BrandingViewController.
@protocol BrandingViewControllerDelegate

// Invoked when the user has tapped on the branding icon.
- (void)brandingIconPressed;

@end

// Creates and manages a view with branding icon. This will show at the left
// side of `FormInputAccessoryViewController` when autofill branding should be
// shown.
@interface BrandingViewController : UIViewController

// Instances an object with the desired delegate.
//
// @param delegate The delegate for this object.
// @return A fresh object with the passed delegate.
- (instancetype)initWithDelegate:(id<BrandingViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_FORM_INPUT_ACCESSORY_BRANDING_VIEW_CONTROLLER_H_
