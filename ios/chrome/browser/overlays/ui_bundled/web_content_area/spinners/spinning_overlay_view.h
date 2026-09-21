// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_VIEW_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_VIEW_H_

#import <UIKit/UIKit.h>

@protocol SpinningOverlayViewDelegate;

// View displaying a semi-transparent background, an activity indicator, and an
// optional label indicating progress or cancellation instructions.
@interface SpinningOverlayView : UIView

// The delegate for handling tap events.
@property(nonatomic, weak) id<SpinningOverlayViewDelegate> delegate;

// Initializes the view with optional `labelText` and whether tapping cancels
// the overlay.
- (instancetype)initWithLabelText:(NSString*)labelText
                      cancellable:(BOOL)cancellable NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_WEB_CONTENT_AREA_SPINNERS_SPINNING_OVERLAY_VIEW_H_
