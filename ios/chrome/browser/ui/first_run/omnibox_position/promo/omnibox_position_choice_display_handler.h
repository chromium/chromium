// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

/// Handler for displaying the OmniboxPositionChoice screen. Called by the
/// PromosManager.
@interface OmniboxPositionChoiceDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

/// PromosManagerCommands handler.
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_OMNIBOX_POSITION_PROMO_OMNIBOX_POSITION_CHOICE_DISPLAY_HANDLER_H_
