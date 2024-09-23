// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"
#include "ios/chrome/browser/history/ui_bundled/history_consumer.h"
#import "ios/chrome/browser/history/ui_bundled/history_entry_item_delegate.h"

// LegacyChromeTableViewController for displaying history items.
@interface HistoryTableViewController
    : BaseHistoryViewController <UIAdaptivePresentationControllerDelegate>
// Optional: If provided, search terms to filter the displayed history items.
// `searchTerms` will be the initial value of the text in the search bar.
@property(nonatomic, copy) NSString* searchTerms;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_TABLE_VIEW_CONTROLLER_H_
