// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_file_preview_coordinator.h"

#import <QuickLook/QuickLook.h>
#import <UIKit/UIKit.h>

#import "base/check.h"

@interface DownloadFilePreviewCoordinator () <QLPreviewControllerDataSource,
                                              QLPreviewControllerDelegate>
@end

@implementation DownloadFilePreviewCoordinator {
  // The currently presented QLPreviewController, if any.
  QLPreviewController* _previewController;

  // The file URL currently being previewed.
  NSURL* _currentFileURL;
}

#pragma mark - ChromeCoordinator

- (void)stop {
  [super stop];
  [self dismissPreviewIfPresented];
  _currentFileURL = nil;
}

#pragma mark - Public Methods

- (void)presentFilePreviewWithURL:(NSURL*)fileURL {
  CHECK(self.baseViewController);
  CHECK(fileURL);

  _currentFileURL = fileURL;

  // If a preview controller is already presented, just reload the data.
  if (_previewController) {
    [_previewController reloadData];
    return;
  }

  // Create and present new preview controller.
  _previewController = [[QLPreviewController alloc] init];
  _previewController.dataSource = self;
  _previewController.delegate = self;

  [self.baseViewController presentViewController:_previewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - Private Methods

- (void)dismissPreviewIfPresented {
  [_previewController dismissViewControllerAnimated:NO completion:nil];
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:
    (QLPreviewController*)controller {
  return 1;
}

- (id<QLPreviewItem>)previewController:(QLPreviewController*)controller
                    previewItemAtIndex:(NSInteger)index {
  return _currentFileURL;
}

#pragma mark - QLPreviewControllerDelegate

- (void)previewControllerDidDismiss:(QLPreviewController*)controller {
  // Clean up when preview is dismissed by user.
  _currentFileURL = nil;
  _previewController = nil;
}

@end
