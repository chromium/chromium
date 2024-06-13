// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"

NSString* const OmniboxPopupActionsRowCellReuseIdentifier =
    @"OmniboxPopupActionsRowCell";

@protocol OmniboxPopupActionsRowDelegate;
@class SuggestAction;

/// Content configuration of the omnibox popup actions row.
@interface OmniboxPopupActionsRowContentConfiguration
    : OmniboxPopupRowContentConfiguration <OmniboxKeyboardDelegate,
                                           OmniboxReturnDelegate>

- (instancetype)init NS_UNAVAILABLE;
// The available actions attached to the suggestion.
@property(nonatomic, strong, readonly) NSArray<SuggestAction*>* actions;
// The highlighted action index.
@property(nonatomic, readonly) NSUInteger highlightedActionIndex;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_
