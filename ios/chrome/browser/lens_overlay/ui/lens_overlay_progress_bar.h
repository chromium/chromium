// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PROGRESS_BAR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PROGRESS_BAR_H_

#import <UIKit/UIKit.h>

// Progress bar for Lens Overlay
@interface LensOverlayProgressBar : UIProgressView

// Sets the progress, with an optional animation and completion block.
- (void)setProgress:(float)progress
           animated:(BOOL)animated
         completion:(void (^)(BOOL finished))completion;

// Sets the hidden state, with an optional animation and completion block.
- (void)setHidden:(BOOL)hidden
         animated:(BOOL)animated
       completion:(void (^)(BOOL finished))completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_PROGRESS_BAR_H_
