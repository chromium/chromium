// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_UTILS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_UTILS_H_

#import <UIKit/UIKit.h>

@class SelfSizingTableView;
@class TableViewTextHeaderFooterView;

// Dequeues a UITableViewCell from the table view and configures it
// appropriately for the Privacy Guide.
UITableViewCell* PrivacyGuideExplanationCell(UITableView* table_view,
                                             int text_id,
                                             NSString* symbol_name);

// Dequeues a UITableViewCell from the table view and configures it
// appropriately for the Privacy Guide.
UITableViewCell* PrivacyGuideSwitchCell(UITableView* table_view,
                                        int text_id,
                                        BOOL switch_on,
                                        NSString* accessibility_id,
                                        id target,
                                        SEL selector);

// Dequeues a TableViewTextHeaderFooterView from the table view and configures
// it appropriately for the Privacy Guide.
TableViewTextHeaderFooterView* PrivacyGuideHeaderView(UITableView* table_view,
                                                      int text_id);

// Creates a SelfSizingTableView and configures is appropriately for the Privacy
// Guide.
SelfSizingTableView* PrivacyGuideTableView();

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_UTILS_H_
