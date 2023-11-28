// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// A class that generates snapshot images for the WebState associated with this
// class. This lives on the UI thread.
@interface SnapshotGenerator : NSObject

// The SnapshotGenerator delegate.
@property(nonatomic, weak) id<SnapshotGeneratorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Asynchronously generates a new snapshot with WebKit-based snapshot API and
// runs a callback with the new snapshot image. It is an error to call this
// method if the web state is showing anything other (e.g., native content) than
// a web view.
- (void)generateWKWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion;

// Generates and returns a new snapshot image with UIKit-based snapshot API.
- (UIImage*)generateUIViewSnapshot;

// Generates and returns a new snapshot image with UIKit-based snapshot API. The
// generated image includes overlays (e.g., infobars, the download manager, and
// sad tab view).
- (UIImage*)generateUIViewSnapshotWithOverlays;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_GENERATOR_H_
