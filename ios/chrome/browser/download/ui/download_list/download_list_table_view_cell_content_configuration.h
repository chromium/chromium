// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

@class DownloadListItem;

// Configuration object for a download list table view cell.
// This contains a TableViewCellContentConfiguration and adds download-specific
// properties like progress indication and cancel button support.
//
// Visual Layout Diagram:
// ┌───────────────────────────────────────────────────────────────────────────┐
// │                    DownloadListTableViewCell                              │
// │                                                                           │
// │ ┌─────────────┐ ┌────────────────────────────────────────┐ ┌────────────┐ │
// │ │   Leading   │ │ Title (Download filename)              │ │   Cancel   │ │
// │ │    Icon     │ │                                        │ │   Button   │ │
// │ │ (File type) │ │ Subtitle (File size, status)           │ │ (Optional) │ │
// │ └─────────────┘ └────────────────────────────────────────┘ └────────────┘ │
// │                                                                           │
// │                 ┌────────────────────────────────────────┐                │
// │                 │            Progress Bar                │                │
// │                 │ ████████████████░░░░░░░░░░░░░░░░░░░░░░ │                │
// │                 │ [█████ progressTintColor █████][░░░░░] │                │
// │                 │        (0.0 - 1.0 progress)            │                │
// │                 └────────────────────────────────────────┘                │
// │                          (showProgress = YES)                             │
// └───────────────────────────────────────────────────────────────────────────┘
//
// Layout Components:
// • Base TableViewCellContentConfiguration provides:
//   - Leading view (file type icon)
//   - Title (download filename)
//   - Subtitle (file size, download status)
//   - Trailing view (cancel button when showCancelButton = YES)
//
// • Download-specific additions:
//   - Progress bar (when showProgress = YES)
//     * progress: CGFloat (0.0 to 1.0)
//     * progressTintColor: UIColor (default system blue)
//     * progressTrackTintColor: UIColor (default system gray)
//   - Cancel button functionality (when showCancelButton = YES)
//
// States:
// 1. Downloading: showProgress = YES, showCancelButton = YES
// 2. Completed: showProgress = NO, showCancelButton = NO
// 3. Failed: showProgress = NO, showCancelButton = NO (with error subtitle)
// 4. Paused: showProgress = YES, showCancelButton = YES (with paused subtitle)
//
@interface DownloadListTableViewCellContentConfiguration
    : NSObject <UIContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The underlying table view cell content configuration.
@property(nonatomic, strong)
    TableViewCellContentConfiguration* cellContentConfiguration;

// Whether to show a progress indicator for ongoing downloads. Default NO.
@property(nonatomic, assign) BOOL showProgress;

// The download progress value (0.0 to 1.0). Only used when showProgress is YES.
@property(nonatomic, assign) CGFloat progress;

// Whether to show a cancel button for cancellable downloads. Default NO.
@property(nonatomic, assign) BOOL showCancelButton;

// The progress tint color. Default is system blue.
@property(nonatomic, strong) UIColor* progressTintColor;

// The track tint color for progress. Default is system gray.
@property(nonatomic, strong) UIColor* progressTrackTintColor;

// LINT.ThenChange(download_list_table_view_cell_content_configuration.mm:Copy)

// Registers/Dequeues a TableViewCell for this content configuration. This
// ensures that the pool of cells that will be used for this content
// configuration can be reused for the same configurations.
+ (void)registerCellForTableView:(UITableView*)tableView;
+ (UITableViewCell*)dequeueTableViewCell:(UITableView*)tableView;

// Convenience initializer to create a configuration from a DownloadListItem.
+ (instancetype)configurationWithDownloadListItem:(DownloadListItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CELL_CONTENT_CONFIGURATION_H_
