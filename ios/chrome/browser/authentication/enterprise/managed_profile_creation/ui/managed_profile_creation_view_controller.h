// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_UI_MANAGED_PROFILE_CREATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_UI_MANAGED_PROFILE_CREATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/enterprise/managed_profile_creation/ui/managed_profile_creation_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

namespace signin {
enum class ManagedAccountSigninMode;
}  // namespace signin

// Delegate of Managed profile creation view controller.
@protocol ManagedProfileCreationViewControllerDelegate <
    PromoStyleViewControllerDelegate>

- (void)showMergeBrowsingDataScreen;

@end

@protocol ManagedProfileCreationDataSource

// Returns the way the view should be presented.
@property(nonatomic, readonly) signin::ManagedAccountSigninMode mode;

@end

// View controller of managed profile creation screen.
@interface ManagedProfileCreationViewController
    : PromoStyleViewController <ManagedProfileCreationConsumer>

@property(nonatomic, weak) id<ManagedProfileCreationViewControllerDelegate>
    managedProfileCreationViewControllerPresentationDelegate;

@property(nonatomic, weak) id<ManagedProfileCreationDataSource>
    managedProfileCreationDataSource;

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_UI_MANAGED_PROFILE_CREATION_VIEW_CONTROLLER_H_
