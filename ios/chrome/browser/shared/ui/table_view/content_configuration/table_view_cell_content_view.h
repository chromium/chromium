// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@class TableViewCellContentConfiguration;

@interface TableViewCellContentView : UIView <ChromeContentView>

/// Initializes with `configuration`.
- (instancetype)initWithConfiguration:
    (TableViewCellContentConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns the trailing content view, to be used in testing only.
- (UIView*)trailingContentViewForTesting;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_TABLE_VIEW_CELL_CONTENT_VIEW_H_
