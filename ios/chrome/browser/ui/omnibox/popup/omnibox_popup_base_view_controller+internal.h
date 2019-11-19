// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_BASE_VIEW_CONTROLLER_INTERNAL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_BASE_VIEW_CONTROLLER_INTERNAL_H_

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller.h"

// Interface defining protected methods on OmniboxPopupBaseViewController.
//
// TODO (crbug.com/943521): This is only for subclassing during migration of
// OmniboxPopupViewController and should be removed when that is done.
@interface OmniboxPopupBaseViewController (Internal) <UITableViewDelegate>

// Alignment of omnibox text. Popup text should match this alignment.
@property(nonatomic, assign) NSTextAlignment alignment;

// Semantic content attribute of omnibox text. Popup should match this
// attribute. This is used by the new omnibox popup.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

// Table view that displays the results.
@property(nonatomic, strong) UITableView* tableView;

// Adjust the inset on the table view to prevent keyboard from overlapping the
// text.
- (void)updateContentInsetForKeyboard;

// Hook for subclasses to update the table view after the matches have been
// processed.
- (void)updateTableViewWithAnimation:(BOOL)animation;

// Action handler for when the button is tapped.
- (void)trailingButtonTapped:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_BASE_VIEW_CONTROLLER_INTERNAL_H_
