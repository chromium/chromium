// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/callback.h"
#import "ios/public/provider/chrome/browser/lens/lens_image_metadata.h"
#import "ios/public/provider/chrome/browser/lens/lens_query.h"
#import "ios/web/public/navigation/navigation_manager.h"

@class LensConfiguration;
class GURL;
enum class LensEntrypoint;

#pragma mark - Lens View Finder

@protocol ChromeLensViewFinderController;

// A delegate that can receive Lens events forwarded by a
// `ChromeLensViewFinderController`.
@protocol ChromeLensViewFinderDelegate <NSObject>

// Called when the Lens view controller's dimiss button has been tapped.
- (void)lensControllerDidTapDismissButton:
    (id<ChromeLensViewFinderController>)lensController;

// Called when the user selects a URL in Lens.
- (void)lensController:(id<ChromeLensViewFinderController>)lensController
          didSelectURL:(GURL)url;

// Called when the Lens UI is added to a view hierarchy.
- (void)lensControllerWillAppear:
    (id<ChromeLensViewFinderController>)lensController;

// Called after the Lens UI was removed from a view hierarchy.
- (void)lensControllerWillDisappear:
    (id<ChromeLensViewFinderController>)lensController;

// Called when the user picked or captured an image.
- (void)lensController:(id<ChromeLensViewFinderController>)lensController
    didSelectImageWithMetadata:(id<LensImageMetadata>)imageMetadata;

@end

// A controller that can facilitate communication with the downstream LVF
// controller.
@protocol ChromeLensViewFinderController <NSObject>

// Sets the delegate for LVF.
- (void)setLensViewFinderDelegate:(id<ChromeLensViewFinderDelegate>)delegate;

// Builds the capture infrastructure for the live camera preview in selection
// filter. This is called on view load and can be called after the UI has been
// torn down to restore.
- (void)buildCaptureInfrastructureForSelection;

// Builds the capture infrastructure for the live camera preview in translate
// filter. This is called on view load and can be called after the UI has been
// torn down to restore.
- (void)buildCaptureInfrastructureForTranslate;

// Deprecated. Use `tearDownCaptureInfrastructureWithPlaceholder:`
// Tears down the live camera preview and destroys the UI.
- (void)tearDownCaptureInfrastructure;

// Tears down the live camera preview and destroys the capture UI.
// This method is invoked with a boolean indicating whether a placeholder UI
// should be displayed after tearing down the capture infrastructure.
- (void)tearDownCaptureInfrastructureWithPlaceholder:(BOOL)showPlaceholder;

@end

namespace ios {
namespace provider {

// Returns a controller for the given configuration that can facilitate
// communication with the downstream Lens View Finder controller.
UIViewController<ChromeLensViewFinderController>*
NewChromeLensViewFinderController(LensConfiguration* config);

// Returns whether Lens is supported for the current build.
bool IsLensSupported();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
