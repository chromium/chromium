// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_mutator.h"

@protocol AutoDeletionPromoCommands;
class PrefService;

// Mediator for the Auto-deletion UI.
@interface AutoDeletionPromoMediator : NSObject <AutoDeletionPromoMutator>

// Handler for local Auto-deletion promo UI commands.
@property(nonatomic, weak) id<AutoDeletionPromoCommands>
    autoDeletionPromoHandler;

// Initializes the mediator with `localState`.
- (instancetype)initWithLocalState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator from model services.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_PROMO_MEDIATOR_H_
