// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_

#import <UIKit/UIKit.h>

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
    suggestSignalsAvailableOnResult:(id<ChromeLensOverlayResult>)result;

// The lens overlay requested to open a URL (e.g. after a selection in the
// flyout menu).
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didRequestToOpenURL:(GURL)URL;

// The lens overlay requested to open the overlay menu.
- (void)lensOverlayDidOpenOverlayMenu:(id<ChromeLensOverlay>)lensOverlay;

@end

// Defines the interface for interacting with a Chrome Lens Overlay.
@protocol ChromeLensOverlay

// Whether the user is currently panning the selection UI.
@property(nonatomic, readonly) BOOL isPanningSelectionUI;

// Sets the delegate for `ChromeLensOverlay`.
- (void)setLensOverlayDelegate:(id<ChromeLensOverlayDelegate>)delegate;

// Called when the text is added into the multimodal omnibox.
// If `clearSelection` is YES, the current visual selection will be cleared.
- (void)setQueryText:(NSString*)text clearSelection:(BOOL)clearSelection;

// Starts executing requests.
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
@end

namespace ios {
namespace provider {

// Creates a controller for the given snapshot that can facilitate
// communication with the downstream Lens controller.
UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    UIImage* snapshot,
    LensConfiguration* config,
    NSArray<UIAction*>* additionalMenuItems);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
