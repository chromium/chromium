// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_assistant_button_menu_factory.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_assistant_button_symbol.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation AppBarAssistantButtonMenuFactory

- (UIMenu*)menuWithCheckedState:(AppBarAssistantButtonState)checkedState
                  showAskGemini:(BOOL)showAskGemini
                       showLens:(BOOL)showLens
                    showAccount:(BOOL)showAccount {
  NSMutableArray<UIMenuElement*>* actions = [NSMutableArray array];
  if (showAskGemini) {
    [actions addObject:[self actionWithTitleID:IDS_IOS_APP_BAR_ASK_GEMINI
                                preferredState:
                                    AppBarAssistantButtonPreferredState::kAsk
                                  checkedState:checkedState]];
  }
  if (showLens) {
    [actions addObject:[self actionWithTitleID:IDS_IOS_LENS_PRODUCT_NAME
                                preferredState:
                                    AppBarAssistantButtonPreferredState::kLens
                                  checkedState:checkedState]];
  }
  if (showAccount) {
    [actions
        addObject:[self actionWithTitleID:IDS_IOS_APP_BAR_ACCOUNT
                           preferredState:AppBarAssistantButtonPreferredState::
                                              kAccount
                             checkedState:checkedState]];
  }
  return
      [UIMenu menuWithTitle:l10n_util::GetNSString(
                                IDS_IOS_APP_BAR_ASSISTANT_CUSTOMIZATION_TITLE)
                      image:nil
                 identifier:nil
                    options:UIMenuOptionsSingleSelection
                   children:actions];
}

#pragma mark - Private

// Returns the menu action titled with the `titleID` string, which selects
// `preferredState` as the preferred assistant button state. `preferredState`
// must not be `kDefault`. The action is checked when the assistant button state
// matching `preferredState` is `checkedState`. Its icon is the one of the
// assistant button in that state, so that it previews the button resulting from
// its selection. For Account, the generic symbol is used instead of the avatar,
// as the entry describes the kind of button, not the signed-in identity.
- (UIAction*)actionWithTitleID:(int)titleID
                preferredState:
                    (AppBarAssistantButtonPreferredState)preferredState
                  checkedState:(AppBarAssistantButtonState)checkedState {
  AppBarAssistantButtonState state =
      AssistantButtonStateFromPreferredState(preferredState).value();
  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction
      actionWithTitle:l10n_util::GetNSString(titleID)
                image:SymbolWithPointSize(AppBarAssistantButtonSymbol(state),
                                          kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* uiAction) {
                [weakSelf.mutator
                    setPreferredAssistantButtonState:preferredState];
              }];
  action.state =
      state == checkedState ? UIMenuElementStateOn : UIMenuElementStateOff;
  return action;
}

@end
