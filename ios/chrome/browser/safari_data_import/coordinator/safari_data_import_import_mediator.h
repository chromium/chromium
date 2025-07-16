/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol SafariDataImportImportStageConsumer;
@protocol SafariDataItemConsumer;

/// Mediator for the safari data import screen. Handles stages of importing a
/// .zip file generated from Safari data to Chrome.
@interface SafariDataImportImportMediator : NSObject <UIDocumentPickerDelegate>

/// Consumer object handling import stage transitions.
@property(nonatomic, weak) id<SafariDataImportImportStageConsumer>
    importStageConsumer;

/// Consumer object displaying Safari item import status.
@property(nonatomic, weak) id<SafariDataItemConsumer> itemConsumer;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_MEDIATOR_H_
