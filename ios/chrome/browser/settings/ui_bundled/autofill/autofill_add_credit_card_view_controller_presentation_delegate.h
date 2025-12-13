// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutofillAddCreditCardViewController;

// Delegate for presentation events related to
// AutofillAddCreditCardViewController.
@protocol AddCreditCardViewControllerPresentationDelegate <NSObject>

// Notifies the class which conforms to this protocol that the 'Use Camera'
// button has been tapped on.
- (void)addCreditCardViewControllerRequestedCameraScan:
    (AutofillAddCreditCardViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
