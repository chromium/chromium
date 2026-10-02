// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_MENU_FACTORY_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_MENU_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"

@protocol AppBarMutator;

// Factory creating the menu letting the user choose the entry point shown by
// the AppBar assistant button.
@interface AppBarAssistantButtonMenuFactory : NSObject

// Receives the entry point selected by the user in the menu.
@property(nonatomic, weak) id<AppBarMutator> mutator;

// Returns the menu, with the entry matching `checkedState` checked. The "Ask
// Gemini", "Lens" and "Account" entries are only present when `showAskGemini`,
// `showLens` and `showAccount` are respectively `YES`.
- (UIMenu*)menuWithCheckedState:(AppBarAssistantButtonState)checkedState
                  showAskGemini:(BOOL)showAskGemini
                       showLens:(BOOL)showLens
                    showAccount:(BOOL)showAccount;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_ASSISTANT_BUTTON_MENU_FACTORY_H_
