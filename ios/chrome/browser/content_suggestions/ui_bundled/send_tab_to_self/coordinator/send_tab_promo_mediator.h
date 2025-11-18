// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/standalone_module_delegate.h"

class FaviconLoader;
@protocol MagicStackModuleContainerDelegate;
class PrefService;
@class SendTabPromoItem;
@protocol SendTabPromoMediatorDelegate;

// Mediator for managing the state of the Send Tab to Self Promo Magic Stack
// module.
@interface SendTabPromoMediator : NSObject <StandaloneModuleDelegate>

// Delegate used to communicate events back to the owner of this
// class.
@property(nonatomic, weak) id<SendTabPromoMediatorDelegate> delegate;

// Delegate used to communicate notification events back to the owner of this
// class.
@property(nonatomic, weak) id<MagicStackModuleContainerDelegate>
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

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_PROMO_MEDIATOR_H_
