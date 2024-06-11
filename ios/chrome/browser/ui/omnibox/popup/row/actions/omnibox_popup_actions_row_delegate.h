// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_DELEGATE_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupActionsRowContentConfiguration;
@class SuggestAction;

/// Actions popup row delegate.
@protocol OmniboxPopupActionsRowDelegate
/// The given action was tapped.
- (void)omniboxPopupRowActionSelectedWithConfiguration:
            (OmniboxPopupActionsRowContentConfiguration*)configuration
                                                action:(SuggestAction*)action;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_OMNIBOX_POPUP_ACTIONS_ROW_DELEGATE_H_
