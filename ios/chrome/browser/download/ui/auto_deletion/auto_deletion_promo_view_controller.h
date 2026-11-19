// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol AutoDeletionPromoCommands;
@protocol AutoDeletionPromoMutator;

// The `UIViewController` that manages the Auto-deletion promo's view.
@interface AutoDeletionPromoViewController : UIViewController

// Mutator that handles Auto-deletion model updates.
@property(nonatomic, weak) id<AutoDeletionPromoMutator> mutator;

// Handler for local Auto-deletion promo UI commands.
@property(nonatomic, weak) id<AutoDeletionPromoCommands>
    autoDeletionPromoHandler;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_VIEW_CONTROLLER_H_
