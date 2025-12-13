// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/download/ui/download_list/download_list_action_delegate.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_header.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_record_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol DownloadListMutator;

/// Table view controller for displaying a list of downloads.
@interface DownloadListTableViewController
    : ChromeTableViewController <DownloadListConsumer,
                                 UIAdaptivePresentationControllerDelegate>

/// Command handler for download list actions.
@property(nonatomic, weak) id<DownloadListCommands> downloadListHandler;

/// Command handler for individual download actions.
@property(nonatomic, weak) id<DownloadRecordCommands> downloadRecordHandler;

/// Mutator for handling data operations.
@property(nonatomic, weak) id<DownloadListMutator> mutator;

/// Delegate for handling download actions.
@property(nonatomic, weak) id<DownloadListActionDelegate> actionDelegate;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_LIST_TABLE_VIEW_CONTROLLER_H_
