// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <vector>

@protocol UnifiedConsentViewControllerDelegate;

// UnifiedConsentViewController is a sub view controller to ask for the user
// consent before the user can sign-in.
// All the string ids displayed by the view are available with
// `consentStringIds`. Those can be used to record the consent agreed by the
// user.
@interface UnifiedConsentViewController : UIViewController

@property(nonatomic, weak) id<UnifiedConsentViewControllerDelegate> delegate;
// Returns YES if the consent view is scrolled to the bottom.
@property(nonatomic, assign, readonly) BOOL isScrolledToBottom;

// -[UnifiedConsentViewController initWithPostRestoreSigninPromo:] should be
// used.
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Initializes the instance.
// `postRestoreSigninPromoView` should be set to YES, if the dialog is used for
// post restore sign-in promo.
- (instancetype)initWithPostRestoreSigninPromo:(BOOL)postRestoreSigninPromo
    NS_DESIGNATED_INITIALIZER;

// List of string ids used for the user consent. The string ids order matches
// the way they appear on the screen.
- (const std::vector<int>&)consentStringIds;

// Shows (if hidden) and updates the IdentityButtonControl.
- (void)updateIdentityButtonControlWithUserFullName:(NSString*)fullName
                                              email:(NSString*)email;

// Updates the IdentityButtonControl avatar. If the identity picker view is
// hidden, -[UnifiedConsentViewController
// updateIdentityButtonControlWithUserFullName:email:] has to be called before.
- (void)updateIdentityButtonControlWithAvatar:(UIImage*)avatar;

// Hides the IdentityButtonControl.
- (void)hideIdentityButtonControl;

// Scrolls the consent view to the bottom.
- (void)scrollToBottom;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_H_
