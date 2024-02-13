// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

class AuthenticationService;
@protocol NotificationsOptInPresenter;
@protocol NotificationsOptInConsumer;

// Handles model interactions for the notifications opt-in screen.
@interface NotificationsOptInMediator
    : NSObject <NotificationsOptInViewControllerDelegate,
                PromoStyleViewControllerDelegate>

// NotificationsOptInViewController presenter.
@property(nonatomic, weak) id<NotificationsOptInPresenter> presenter;

// Consumer for this mediator.
@property(nonatomic, weak) id<NotificationsOptInConsumer> consumer;

// Initializes the mediator with the user's pref service to
// manipulate their push notification permissions.
- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Configures the consumer with the user's current notification settings.
- (void)configureConsumer;

// Disables the user's notification opt-in selection for the given
// itemIdentifier.
- (void)disableUserSelectionForItem:
    (NotificationsOptInItemIdentifier)itemIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_NOTIFICATIONS_OPT_IN_MEDIATOR_H_
