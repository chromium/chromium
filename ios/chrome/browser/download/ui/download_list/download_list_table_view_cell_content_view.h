// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@class DownloadListTableViewCellContentConfiguration;
@class TableViewCellContentView;

// Content view for download list table view cells.
// This view contains a TableViewCellContentView and adds a progress view
// that aligns with the leading and trailing views.
@interface DownloadListTableViewCellContentView : UIView <ChromeContentView>

// Designated initializer.
- (instancetype)initWithConfiguration:
    (DownloadListTableViewCellContentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_VIEW_H_
