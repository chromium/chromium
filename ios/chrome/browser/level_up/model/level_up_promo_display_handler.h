// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_display_handler.h"

@protocol LevelUpCommands;

// Handler for displaying Level Up promo. This handler is called by the Promos
// Manager once if user has not seen the promo from overflow menu.
@interface LevelUpPromoDisplayHandler : NSObject <StandardPromoDisplayHandler>

/// Initializer.
- (instancetype)initWithLevelUpCommandsHandler:
    (id<LevelUpCommands>)levelUpCommandsHandler NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end
#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_PROMO_DISPLAY_HANDLER_H_
