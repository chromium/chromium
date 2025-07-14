// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_import_mediator.h"

#import "ios/chrome/browser/safari_data_import/public/safari_data_import_stage.h"
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
  /// TODO(crbug.com/420703283): Import the file.
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  [self.importStageConsumer
      transitionToImportStage:SafariDataImportStage::kNotStarted];
}

@end
