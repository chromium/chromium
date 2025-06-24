// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_MEDIATOR_H_

#import <Foundation/Foundation.h>

class PromosManager;
@protocol UIBlockerTarget;

/// A mediator for the safari data import screen entry point.
@interface SafariDataImportMainMediator : NSObject

/// Initializer.
- (instancetype)initWithUIBlockerTarget:(id<UIBlockerTarget>)target
                          promosManager:(PromosManager*)promosManager
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Displays the entry point again in a few days.
- (void)registerReminder;

/// Disconnects mediator dependencies; should be called when stopping the
/// coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_MEDIATOR_H_
