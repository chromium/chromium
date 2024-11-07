// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import "components/lens/lens_overlay_dismissal_source.h"
#import "components/lens/lens_overlay_first_interaction_type.h"
#import "components/lens/lens_overlay_metrics.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"

namespace web {
class WebState;
}  // namespace web

/// Utility for recording lens overlay related metrics.
@interface LensOverlayMetricsRecorder : NSObject

/// Creates an instance for an associated entrypoint and the webState where the
/// overlay is invoked on. The `associatedWebState` is not retained.
- (instancetype)initWithEntrypoint:(LensOverlayEntrypoint)entrypoint
                associatedWebState:(web::WebState*)associatedWebState;

/// Sets whether the lens overlay is in foreground or not.
- (void)setLensOverlayInForeground:(BOOL)lensOverlayInForeground;

/// Records consent shown.
- (void)recordLensOverlayConsentShown;

/// Records overlay closed.
- (void)recordLensOverlayClosed;

/// Records consent flow presented.
- (void)recordPermissionRequestedToBeShown;

/// Records link opened.
- (void)recordPermissionsLinkOpen;

/// Records permissions denied.
- (void)recordPermissionsDenied;

/// Records permissions accepted.
- (void)recordPermissionsAccepted;

/// Records overflow menu opened.
- (void)recordOverflowMenuOpened;

/// Records that a result was loaded.
- (void)recordResultLoadedWithTextSelection:(BOOL)isTextSelection;

/// Records metrics on lens overlay dismissal.
- (void)recordDismissalMetricsWithSource:
            (lens::LensOverlayDismissalSource)dismissalSource
                       generatedTabCount:(NSInteger)generatedTabCount;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_METRICS_RECORDER_H_
