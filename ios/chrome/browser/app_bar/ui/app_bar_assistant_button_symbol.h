// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_SYMBOL_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_SYMBOL_H_

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

// Returns the symbol shown by the AppBar assistant button in `state`. Shared
// by the button and its customization menu, so that each menu entry previews
// the button resulting from its selection. For `kAccount`, this is the generic
// symbol shown when there is no avatar for the signed-in identity.
Symbol AppBarAssistantButtonSymbol(AppBarAssistantButtonState state);

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_SYMBOL_H_
