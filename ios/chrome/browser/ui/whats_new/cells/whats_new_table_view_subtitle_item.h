// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_SUBTITLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_SUBTITLE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

// WhatsNewTableViewSubtitleItem is a model class that uses
// WhatsNewTableViewSubtitleCell.
@interface WhatsNewTableViewSubtitleItem : TableViewHeaderFooterItem

// The title text string.
@property(nonatomic, strong) NSString* title;

@end

// WhatsNewTableViewSubtitleCell implements a TableViewHeaderFooterView subclass
// containing informative text for What's New.
@interface WhatsNewTableViewSubtitleCell : UITableViewHeaderFooterView

// UILabels corresponding to `title` from the item.
@property(nonatomic, strong) UILabel* textLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_CELLS_WHATS_NEW_TABLE_VIEW_SUBTITLE_ITEM_H_
