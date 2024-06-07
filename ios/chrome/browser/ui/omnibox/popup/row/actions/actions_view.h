// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_ACTIONS_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_ACTIONS_VIEW_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupActionsRowContentConfiguration;

// A scroll view that displays a list of action buttons (eg. Directions)
@interface ActionsView : UIView

- (instancetype)initWithConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration;

// Updates the actions view with a new popup action row configuration.
- (void)updateConfiguration:(OmniboxPopupActionsRowContentConfiguration*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_ACTIONS_ACTIONS_VIEW_H_
