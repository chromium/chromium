// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/snapshots/model/model_swift.h"

class SnapshotID;
@protocol SnapshotStorage;
@class LegacySnapshotGenerator;
@protocol SnapshotGeneratorDelegate;

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page. This lives on the UI thread.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotManager : NSObject <SnapshotManager>

// Designated initializer.
- (instancetype)initWithGenerator:(LegacySnapshotGenerator*)generator
                       snapshotID:(SnapshotID)snapshotID
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_MANAGER_H_
