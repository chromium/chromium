// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_BLUR_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_BLUR_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// A view controller that displays a blur effect.
@interface ReaderModeBlurOverlayViewController : UIViewController

// Animates the blur effect in.
- (void)animateInWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_BLUR_OVERLAY_VIEW_CONTROLLER_H_
