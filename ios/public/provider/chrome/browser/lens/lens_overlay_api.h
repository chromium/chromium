// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/lens/lens_image_source.h"

class GURL;

@protocol ChromeLensOverlayResult;
@protocol ChromeLensOverlay;
@class LensConfiguration;

@protocol ChromeLensOverlayDelegate

// The lens overlay started searching for a result.
- (void)lensOverlayDidStartSearchRequest:(id<ChromeLensOverlay>)lensOverlay;

// The lens overlay search request produced an error.
- (void)lensOverlayDidReceiveError:(id<ChromeLensOverlay>)lensOverlay;

// The lens overlay search request produced a valid result.
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didGenerateResult:(id<ChromeLensOverlayResult>)result;

// The user tapped on the close button in the Lens overlay.
- (void)lensOverlayDidTapOnCloseButton:(id<ChromeLensOverlay>)lensOverlay;

// The lens overlay has suggest signals available for the given result.
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    hasSuggestSignalsAvailableOnResult:(id<ChromeLensOverlayResult>)result;

// The lens overlay requested to open a URL (e.g. after a selection in the
// flyout menu).
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didRequestToOpenURL:(GURL)URL;

// The lens overlay requested to open the overlay menu.
- (void)lensOverlayDidOpenOverlayMenu:(id<ChromeLensOverlay>)lensOverlay;

// The lens overlay has deferred a gesture.
- (void)lensOverlayDidDeferGesture:(id<ChromeLensOverlay>)lensOverlay;

@optional
// The lens overlay failed to detect translatable text.
- (void)lensOverlayDidFailDetectingTranslatableText:
    (id<ChromeLensOverlay>)lensOverlay;

@end

// Defines the interface for interacting with a Chrome Lens Overlay.
@protocol ChromeLensOverlay

// The size of the base image in points.
@property(nonatomic, readonly) CGSize imageSize;

// Whether the current mode is translate.
//
// Note: this method will always return `NO` until the overlay is started.
@property(nonatomic, readonly) BOOL translateFilterActive;

// The layout guide that demarcates the start of the unobstructed area.
@property(nonatomic, strong) UILayoutGuide* visibleAreaLayoutGuide;

// The selection rect in the coordinate system of the query image.
@property(nonatomic, readonly) CGRect selectionRect;

// Sets the delegate for `ChromeLensOverlay`.
- (void)setLensOverlayDelegate:(id<ChromeLensOverlayDelegate>)delegate;

// Called when the text is added into the multimodal omnibox.
// If `clearSelection` is YES, the current visual selection will be cleared.
- (void)setQueryText:(NSString*)text clearSelection:(BOOL)clearSelection;

// Starts executing requests. Subsequent calls after the first one are no-op.
- (void)start;

// Reloads a previous result in the overlay.
- (void)reloadResult:(id<ChromeLensOverlayResult>)result;

// Removes the current selection and optionally clears the query text.
- (void)removeSelectionWithClearText:(BOOL)clearText;

// Updates the occluder insets. If there is a current selection, the scrollview
// may update to satisfy the new insets (optionally animated).
- (void)setOcclusionInsets:(UIEdgeInsets)occlusionInsets
                reposition:(BOOL)reposition
                  animated:(BOOL)animated;

// Resets the selection area to the initial position.
- (void)resetSelectionAreaToInitialPosition:(void (^)())completion;

// Hides the user selected region/text without resetting to initial position.
// Currently, there is no API to unhide the selection.
- (void)hideUserSelection;

// Updates the visibility of the top icons.
- (void)setTopIconsHidden:(BOOL)hidden;

// Updates the visibility of the HUD view.
- (void)setHUDViewHidden:(BOOL)hidden;

// Updates the visibility of the guidance view.
- (void)setGuidanceViewHidden:(BOOL)hidden;

// Disables flyout menus from displaying.
- (void)disableFlyoutMenu:(BOOL)disable;

// Sets the rest height of the guidance view.
//
// The guidance view represents the short educational message shown in the
// bottom half of the screen.
- (void)setGuidanceRestHeight:(CGFloat)height;

// Shows the overflow menu tooltip.
- (void)requestShowOverflowMenuTooltip;

/// Updates the visibility of the guidance view.
- (void)updateGuidanceViewVisibility:(BOOL)visible animated:(BOOL)animated;

/// Zooms the image to the center of the view with the given insets without
/// animation.
- (void)zoomImageToCenter:(UIEdgeInsets)insets;

@end

namespace ios {
namespace provider {

// Creates a controller for the given snapshot that can facilitate
// communication with the downstream Lens controller.
UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    LensImageSource* imageSource,
    LensConfiguration* config,
    NSArray<UIAction*>* precedingMenuItems,
    NSArray<UIAction*>* additionalMenuItems);

UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    LensImageSource* imageSource,
    LensConfiguration* config,
    NSArray<UIAction*>* additionalMenuItems);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
