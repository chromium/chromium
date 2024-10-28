// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/standalone_module_delegate.h"

class FaviconLoader;
@protocol NotificationsModuleDelegate;
class PrefService;
@class SendTabPromoItem;

// Delegate handling events from the SendTabPromoMediator.
@protocol SendTabPromoMediatorDelegate

// Signals that the user has received a tab sent from one of their other
// devices.
- (void)sentTabReceived;

// Signals that the Send Tab Promo Module should be removed.
- (void)removeSendTabPromoModule;

@end

// Mediator for managing the state of the Send Tab to Self Promo Magic Stack
// module.
@interface SendTabPromoMediator : NSObject <StandaloneModuleDelegate>

// Delegate used to communicate events back to the owner of this
// class.
@property(nonatomic, weak) id<SendTabPromoMediatorDelegate> delegate;

// Delegate used to communicate notification events back to the owner of this
// class.
@property(nonatomic, weak) id<NotificationsModuleDelegate>
    notificationsDelegate;

// Default initializer.
- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                          prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Hides the send tab promo module.
- (void)dismissModule;

- (void)disconnect;

// Data for send tab promo to show. Includes the image for the
// latest sent tab to be displayed.
- (SendTabPromoItem*)sendTabPromoItemToShow;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_MEDIATOR_H_
