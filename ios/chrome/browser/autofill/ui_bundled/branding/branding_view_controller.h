// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_consumer.h"

#import <UIKit/UIKit.h>

// Protocol to handle events and interactions in a BrandingViewController.
@protocol BrandingViewControllerDelegate;

// Creates and manages a view with branding icon. This will show at the leading
// side of `FormInputAccessoryViewController` when autofill branding should be
// shown.
@interface BrandingViewController : UIViewController <BrandingConsumer>

// Delegate to handle interactions.
@property(nonatomic, weak) id<BrandingViewControllerDelegate> delegate;

// Property that shows if any other keyboard accessory is visible on the
// keyboard.
@property(nonatomic, assign) BOOL keyboardAccessoryVisible;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_VIEW_CONTROLLER_H_
