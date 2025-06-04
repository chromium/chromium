// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_REMINDER_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_REMINDER_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/standard_promo_display_handler.h"

@protocol ApplicationCommands;
@protocol PromosManagerUIHandler;

/// Handler for displaying the reminder to import data from Safari.
///
/// This handler is called by the Promos Manager and presents the Safarai Import
/// entry point to eligible users.
@interface SafariDataImportReminderPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

/// Initializer.
- (instancetype)initWithApplicationCommandsHandler:
                    (id<ApplicationCommands>)applicationHandler
                            promosManagerUIHandler:(id<PromosManagerUIHandler>)
                                                       promosManagerUIHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_REMINDER_PROMO_DISPLAY_HANDLER_H_
