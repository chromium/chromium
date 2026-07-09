// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_

#import <UIKit/UIKit.h>

// This protocol should be implemented by the objects that manage the
// presentation context of Lens Overlay.
@protocol LensOverlayPresentationEnvironment <NSObject>

// Returns the required insets for the Lens Overlay presentation.
- (NSDirectionalEdgeInsets)presentationInsetsForLensOverlay;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_PRESENTATION_ENVIRONMENT_H_
