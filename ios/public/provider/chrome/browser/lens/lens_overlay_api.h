// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_

#import <UIKit/UIKit.h>

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

@end

// Defines the interface for interacting with a Chrome Lens Overlay.
@protocol ChromeLensOverlay

// Sets the delegate for `ChromeLensOverlay`.
- (void)setLensOverlayDelegate:(id<ChromeLensOverlayDelegate>)delegate;

// Called when the text is added into the multimodal omnibox.
- (void)setQueryText:(NSString*)text;

// Starts executing requests.
- (void)start;

// Reloads a previous result in the overlay.
- (void)reloadResult:(id<ChromeLensOverlayResult>)result;

@end

namespace ios {
namespace provider {

// Creates a controller for the given snapshot that can facilitate
// communication with the downstream Lens controller.
UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    UIImage* snapshot,
    LensConfiguration* config);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_OVERLAY_API_H_
