// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// A class that generates snapshot images for the WebState associated with this
// class. This lives on the UI thread.
// TODO(crbug.com/40943236): Remove this class once the new implementation
// written in Swift is used by default.
@interface LegacySnapshotGenerator : NSObject

// The SnapshotGenerator delegate.
@property(nonatomic, weak) id<SnapshotGeneratorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Generates a new snapshot and runs a callback with the new snapshot image.
// - If the web state is not showing anything other than a web view (e.g.,
//   native content, incognito tabs) and it doesn't have JavaScript dialogs,
//   - it uses WebKit-based snapshot APIs
//   - and the callback is called asynchronously.
// - Otherwise,
//   - it uses UIKit-based snapshot APIs
//   - and the callback is called immediately (without posting a task).
- (void)generateSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates and returns a new snapshot image with UIKit-based snapshot API.
- (UIImage*)generateUIViewSnapshot;

// Generates and returns a new snapshot image with UIKit-based snapshot API. The
// generated image includes overlays (e.g., infobars, the download manager, and
// sad tab view).
- (UIImage*)generateUIViewSnapshotWithOverlays;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_LEGACY_SNAPSHOT_GENERATOR_H_
