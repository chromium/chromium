// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_result_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_delegate.h"

/// Main mediator for Lens Overlay.
/// Manages data flow between Selection and Results.
@interface LensOverlayMediator : NSObject <LensOverlaySelectionDelegate>

@property(nonatomic, weak) id<LensOverlayResultConsumer> resultConsumer;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_MEDIATOR_H_
