// Copyright 2018 The Chromium Authors. All rights reserved.
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
// |consentStringIds| and |openSettingsStringId|. Those can be used to record
// the consent agreed by the user.
@interface UnifiedConsentViewController : UIViewController

@property(nonatomic, weak) id<UnifiedConsentViewControllerDelegate> delegate;
// String id for text to open the settings (related to record the user consent).
@property(nonatomic, assign, readonly) int openSettingsStringId;
// Returns YES if the consent view is scrolled to the bottom.
@property(nonatomic, assign, readonly) BOOL isScrolledToBottom;

// -[UnifiedConsentViewController init] should be used.
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// List of string ids used for the user consent. The string ids order matches
// the way they appear on the screen.
- (const std::vector<int>&)consentStringIds;

// Shows (if hidden) and updates the IdentityPickerView.
- (void)updateIdentityPickerViewWithUserFullName:(NSString*)fullName
                                           email:(NSString*)email;

// Updates the IdentityPickerView avatar. If the identity picker view is hidden,
// -[UnifiedConsentViewController updateIdentityPickerViewWithUserFullName:
//  email:] has to be called before.
- (void)updateIdentityPickerViewWithAvatar:(UIImage*)avatar;

// Hides the IdentityPickerView.
- (void)hideIdentityPickerView;

// Scrolls the consent view to the bottom.
- (void)scrollToBottom;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_H_
