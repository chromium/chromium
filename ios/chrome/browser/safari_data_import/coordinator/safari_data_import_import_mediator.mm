// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"

#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "ios/chrome/browser/safari_data_import/ui/safari_data_import_import_stage_consumer.h"

@implementation SafariDataImportImportMediator {
  /// Whether the file loading process has started.
  BOOL _fileStartedToLoad;
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  if (_fileStartedToLoad) {
    return;
  }
  _fileStartedToLoad = YES;
  /// TODO(crbug.com/420703283): This is a temporary code snippet for
  /// `SafariDataItem` demonstration purpose. Replace with actual logic to
  /// import the file and call `importPreparationDidComplete`.
  __weak __typeof(self) weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [weakSelf.itemConsumer
            populateItem:[[SafariDataItem alloc]
                             initWithType:SafariDataItemType::kPasswords]];
        [weakSelf.itemConsumer
            populateItem:[[SafariDataItem alloc]
                             initWithType:SafariDataItemType::kHistory]];
        [weakSelf.itemConsumer
            populateItem:[[SafariDataItem alloc]
                             initWithType:SafariDataItemType::kBookmarks]];
        [weakSelf.itemConsumer
            populateItem:[[SafariDataItem alloc]
                             initWithType:SafariDataItemType::kPayment]];
      });
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.importStageConsumer
      transitionToImportStage:SafariDataImportStage::kNotStarted];
}

@end
