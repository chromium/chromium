// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_H_

#import <UIKit/UIKit.h>

// Base class for the TableViewCell used by the TableViewItems.
@interface TableViewCell : UITableViewCell

// Whether custom separator should be used. The separator can replace the
// separator provided by UITableViewCell. It is a 0.5pt high line.
@property(nonatomic, assign) BOOL useCustomSeparator;

// View displayed as custom separator. Use this property to set the leading
// anchor of the custom separator. Default is 16 points (priority high + 1).
@property(nonatomic, strong, readonly) UIView* customSeparator;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_H_
