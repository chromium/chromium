// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_BLUR_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_BLUR_OVERLAY_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for displaying a blur overlay over the current context.
@interface ReaderModeBlurOverlayCoordinator : ChromeCoordinator

// Starts the coordinator with a completion block that is called after the blur
// animation.
- (void)startWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_BLUR_OVERLAY_COORDINATOR_H_
