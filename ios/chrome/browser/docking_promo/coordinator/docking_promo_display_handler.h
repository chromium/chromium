// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

// Handler for displaying the Docking Promo.
//
// This handler is called by the Promos Manager and presents the Docking Promo
// to eligible users.
@interface DockingPromoDisplayHandler : NSObject <StandardPromoDisplayHandler>

// `PromosManagerCommands` handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_COORDINATOR_DOCKING_PROMO_DISPLAY_HANDLER_H_
