// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class TableViewCell;
@class LegacyTableViewCell;

// Configuration object for a TableView cell.
// It is using a TableViewCellContentView as content view.
// +---------------------------------------------+
// |           TableViewCellContentView          |
// |                                             |
// |  title                       trailing label |
// |  subtitle                                   |
// |                                             |
// +---------------------------------------------+
@interface TableViewCellContentConfiguration : NSObject <UIContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The title of the cell.
@property(nonatomic, copy) NSString* title;
@property(nonatomic, strong) UIColor* titleColor;
// Defaults to 0 (unlimited).
@property(nonatomic, assign) NSInteger titleNumberOfLines;

// The leading content configuration for the cell.
@property(nonatomic, copy) id<UIContentConfiguration> leadingConfiguration;

// The subtitle of the cell.
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, strong) UIColor* subtitleColor;
// Defaults to 0 (unlimited).
@property(nonatomic, assign) NSInteger subtitleNumberOfLines;

// The trailing details of the cell.
@property(nonatomic, copy) NSString* trailingText;
@property(nonatomic, strong) UIColor* trailingTextColor;
// Defaults to 1.
@property(nonatomic, assign) NSInteger trailingTextNumberOfLines;

// LINT.ThenChange(table_view_cell_content_configuration.mm:Copy)

// Registers/Dequeues a TableViewCell for this content configuration. This
// ensures that the pool of cells that will be used for this content
// configuration can be reused for the same configurations.
+ (void)registerCellForTableView:(UITableView*)tableView;
+ (TableViewCell*)dequeueTableViewCell:(UITableView*)tableView;

// TODO(crbug.com/443034511): Remove this method.
// **DO NOT use both the legacy and non-legacy versions on the same
// table view**.
+ (void)legacyRegisterCellForTableView:(UITableView*)tableView;
+ (LegacyTableViewCell*)legacyDequeueTableViewCell:(UITableView*)tableView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
