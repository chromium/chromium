// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"

NSString* const OmniboxPopupActionsRowCellReuseIdentifier =
    @"OmniboxPopupActionsRowCell";

@protocol OmniboxPopupActionsRowDelegate;
@class SuggestAction;

/// Content configuration of the omnibox popup row, contains the logic of an
/// actions row UI .
@interface OmniboxPopupActionsRowContentConfiguration
    : OmniboxPopupRowContentConfiguration

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_CONTENT_CONFIGURATION_H_
