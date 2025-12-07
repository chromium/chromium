// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_

#import <UIKit/UIKit.h>

#import "components/lens/lens_overlay_dismissal_source.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"

@protocol LensImageMetadata;
@protocol LensOverlayResultsPagePresenting;
@class LensResultPageViewController;
@class LensOverlayContainerViewController;

// Factory block for creating an object that conforms to the
// LensOverlayResultsPagePresenting protocol.
typedef id<LensOverlayResultsPagePresenting> (^LensResultsPresenterFactory)(
    LensOverlayContainerViewController* baseViewController,
    LensResultPageViewController* resultsViewController);

/// Commands related to Lens Overlay.
@protocol LensOverlayCommands

/// Creates a new Lens UI. Automatically destroys any existing Lens UI as only
/// one instance of it per BVC is supported.
- (void)createAndShowLensUI:(BOOL)animated
                 entrypoint:(LensOverlayEntrypoint)entrypoint
                 completion:(void (^)(BOOL))completion;

/// Responds to a search image with Lens request by creating a new Lens UI with
/// the given image. The overlay will be shown over the specified
/// initial presentation base.
/// The `presenterFactory` allows the caller to provide a custom presenter for
/// the results page. If nil, a default presenter will be used.
/// The completion is called once the UI is presented.
- (void)searchImageWithLens:(UIImage*)image
                 entrypoint:(LensOverlayEntrypoint)entrypoint
    initialPresentationBase:(UIViewController*)initialPresentationBase
    resultsPresenterFactory:(LensResultsPresenterFactory)presenterFactory
                 completion:(void (^)(BOOL))completion;

/// Responds to a search image with Lens request by creating a new Lens UI with
/// the given image. The overlay will be shown over the specified
/// initial presentation base.
/// The completion is called once the UI is presented.
- (void)searchWithLensImageMetadata:(id<LensImageMetadata>)metadata
                         entrypoint:(LensOverlayEntrypoint)entrypoint
            initialPresentationBase:(UIViewController*)initialPresentationBase
                         completion:(void (^)(BOOL))completion;

/// Display the lens overlay, if it exists.
- (void)showLensUI:(BOOL)animated;

/// Hide lens overlay if it exists.
- (void)hideLensUI:(BOOL)animated completion:(void (^)())completion;

/// Destroy lens overlay (called e.g. in response to memory pressure).
- (void)destroyLensUI:(BOOL)animated
               reason:(lens::LensOverlayDismissalSource)dismissalSource;

/// Destroy lens overlay (called e.g. in response to memory pressure).
/// Completion is called when the Lens Overlay is destroyed.
- (void)destroyLensUI:(BOOL)animated
               reason:(lens::LensOverlayDismissalSource)dismissalSource
           completion:(void (^)())completion;

/// Prepares for a tab change that is about to happen in the background.
- (void)prepareLensUIForBackgroundTabChange;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_OVERLAY_COMMANDS_H_
