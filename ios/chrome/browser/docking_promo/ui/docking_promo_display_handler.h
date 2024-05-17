// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

@protocol DockingPromoCommands;

// Handler for displaying the Docking Promo.
//
// This handler is called by the Promos Manager and presents the Docking Promo
// to eligible users.
@interface DockingPromoDisplayHandler : NSObject <StandardPromoDisplayHandler>

// Initializes a promo display handler for the Docking Promo, with the option to
// display the "Remind Me Later" promo version.
- (instancetype)initWithHandler:(id<DockingPromoCommands>)handler
       showRemindMeLaterVersion:(BOOL)showRemindMeLaterVersion;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_DISPLAY_HANDLER_H_
