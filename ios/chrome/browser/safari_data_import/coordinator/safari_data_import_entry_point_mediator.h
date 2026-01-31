// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_ENTRY_POINT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_ENTRY_POINT_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace feature_engagement {
class Tracker;
}
class PromosManager;
class PrefService;
@protocol UIBlockerTarget;

/// A mediator for the safari data import screen entry point.
@interface SafariDataImportEntryPointMediator : NSObject

/// Initializer.
- (instancetype)initWithUIBlockerTarget:(id<UIBlockerTarget>)target
                          promosManager:(PromosManager*)promosManager
               featureEngagementTracker:(feature_engagement::Tracker*)tracker
                            prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Displays the entry point again in a few days.
- (void)registerReminder;

/// Mark Safari data import workflow as used or dismissed by the user.
- (void)notifyUsedOrDismissed;

// Marks the Safari Data Import item in the setup list as completed.
- (void)markSetUpListItemAsComplete;

/// Disconnects mediator dependencies; should be called when stopping the
/// coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_ENTRY_POINT_MEDIATOR_H_
