// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_LIST_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"

@protocol DownloadListConsumer;
class DownloadRecordService;

// Mediator for download list view controller.
@interface DownloadListMediator : NSObject <DownloadListMutator>

#pragma mark - Setup Methods

// Inits with the download record service and incognito status.
- (instancetype)initWithDownloadRecordService:
                    (DownloadRecordService*)downloadRecordService
                                  isIncognito:(BOOL)isIncognito;

// Default initializer is unavailable.
- (instancetype)init NS_UNAVAILABLE;

// Sets the consumer for UI updates.
- (void)setConsumer:(id<DownloadListConsumer>)consumer;

// Disconnects the mediator from the model layer.
- (void)disconnect;

// Connects the mediator to the model layer.
- (void)connect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_DOWNLOAD_LIST_MEDIATOR_H_
