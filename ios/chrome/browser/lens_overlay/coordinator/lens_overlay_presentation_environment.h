// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_

#import <UIKit/UIKit.h>

// This protocol should be implemented by the objects that manage the
// presentation context of Lens Overlay.
// Entities that conform to this protocol will be notified at various stages
// of the Lens Overlay's presentation lifecycle.
@protocol LensOverlayPresentationEnvironment <NSObject>

// Notifies the embedder that Lens Overlay is about to appear.
- (void)lensOverlayWillAppear;

// Notifies the embedder that Lens Overlay is about to disappear.
- (void)lensOverlayWillDisappear;

// Returns the required insets for the Lens Overlay presentation.
- (NSDirectionalEdgeInsets)presentationInsetsForLensOverlay;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_
