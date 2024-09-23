// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COMMANDS_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COMMANDS_H_

@protocol DefaultBrowserPromoNonModalCommands

// Shows the non-modal default promo.
- (void)showDefaultBrowserNonModalPromo;

// Dismisses the non-modal default promo.
- (void)dismissDefaultBrowserNonModalPromoAnimated:(BOOL)animated;

// Alerts the command handler that the non-modal default promo was dismissed
// from the UI.
- (void)defaultBrowserNonModalPromoWasDismissed;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COMMANDS_H_
