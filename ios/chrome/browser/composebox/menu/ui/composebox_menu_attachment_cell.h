// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_CELL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_CELL_H_

#import <UIKit/UIKit.h>

@class ComposeboxMenuItem;

// A cell displaying an attachment option in the composebox menu.
@interface ComposeboxMenuAttachmentCell : UICollectionViewCell

// Configures the cell with the given item.
- (void)configureWithItem:(ComposeboxMenuItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_ATTACHMENT_CELL_H_
