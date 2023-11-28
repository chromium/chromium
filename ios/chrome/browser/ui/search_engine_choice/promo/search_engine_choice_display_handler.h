// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

// TODO(b/306576460): Clean up all promos manager -related code for the choice
// screen once the internal references to this class are deleted.

// Handler for displaying the Search Engine Choice screen. Called by the
// PromosManager.
@interface SearchEngineChoiceDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

// PromosManagerCommands handler
@property(nonatomic, weak) id<PromosManagerCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_PROMO_SEARCH_ENGINE_CHOICE_DISPLAY_HANDLER_H_
