// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui/download_list_consumer.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"

@protocol DownloadListMutator;

// View controller for displaying download list.
@interface DownloadListViewController
    : UITableViewController <DownloadListConsumer,
                             UIAdaptivePresentationControllerDelegate>

// Command handler for download list actions.
@property(nonatomic, weak) id<DownloadListCommands> downloadListHandler;

// Mutator for handling data operations.
@property(nonatomic, weak) id<DownloadListMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_VIEW_CONTROLLER_H_
