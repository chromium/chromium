// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list_view_controller.h"

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/ui/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list_mutator.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"

@interface DownloadListViewController () {
  std::vector<DownloadRecord> _downloadRecords;
  BOOL _isLoading;
  BOOL _isEmpty;
}
@end

@implementation DownloadListViewController

@synthesize mutator = _mutator;

- (void)configureNavigationItem {
  // Create close button for navigation item.
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeButtonTapped)];
  self.navigationItem.rightBarButtonItem = closeButton;

  // Sets title.
  self.title = @"Downloads";
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set up UI elements, table view, etc. will be implemented in a future
  // iteration.

  [self configureNavigationItem];

  // Load records.
  [self.mutator loadDownloadRecords];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
}

#pragma mark - Private

// Dismiss the view controller when close button is tapped.
- (void)closeButtonTapped {
  [self dismissViewControllerAnimated:YES completion:nil];
  [self.downloadListHandler hideDownloadList];
}

#pragma mark - DownloadListConsumer

- (void)setDownloadRecords:(const std::vector<DownloadRecord>&)records {
  _downloadRecords = records;
  // Update UI with new records will be implemented in a future iteration.
}

- (void)setLoadingState:(BOOL)loading {
  _isLoading = loading;
  // Show loading indicator will be implemented in a future iteration.
}

- (void)setEmptyState:(BOOL)empty {
  _isEmpty = empty;
  // Show empty state view will be implemented in a future iteration.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

// Called before the presentation controller will dismiss.
- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [self.downloadListHandler hideDownloadList];
}

@end
