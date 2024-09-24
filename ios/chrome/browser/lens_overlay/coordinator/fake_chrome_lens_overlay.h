// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_H_

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"

@protocol ChromeLensOverlayResult;
class GURL;

/// Class that simulates ChromeLensOverlay behavior for tests.
@interface FakeChromeLensOverlay : NSObject <ChromeLensOverlay>

/// Delegate for ChromeLensOverlay.
@property(nonatomic, weak) id<ChromeLensOverlayDelegate> lensOverlayDelegate;

/// URL that will be used for the next result.
@property(nonatomic, assign) GURL resultURL;

/// Last generated result.
@property(nonatomic, strong) id<ChromeLensOverlayResult> lastResult;
/// Last reloaded result.
@property(nonatomic, strong) id<ChromeLensOverlayResult> lastReload;

/// Simulates a selection update and generate new results.
- (void)simulateSelectionUpdate;

/// Simulates the successful fetch of suggest `signals` for the last selection.
- (void)simulateSuggestSignalsUpdate:(NSData*)signals;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_FAKE_CHROME_LENS_OVERLAY_H_
