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
@class UIViewController;
@class UIImage;
class GURL;
enum class LensEntrypoint;

// A delegate that can receive Lens events forwarded by a ChromeLensController.
@protocol ChromeLensControllerDelegate <NSObject>

// Called when the Lens view controller's dimiss button has been tapped.
- (void)lensControllerDidTapDismissButton;

// Called when the user selects a URL in Lens.
- (void)lensControllerDidSelectURL:(NSURL*)url;

// Called when the user selects an image and the Lens controller has prepared
// `params` for loading a Lens web page.
- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params;

// Called when the user picked or captured an image.
- (void)lensControllerDidGenerateImage:(UIImage*)image;

// Returns the frame of the web content area of the browser.
- (CGRect)webContentFrame;

@end

// A controller that can facilitate communication with the downstream Lens
// controller.
@protocol ChromeLensController <NSObject>

// A delegate that can receive Lens events forwarded by the controller.
@property(nonatomic, weak) id<ChromeLensControllerDelegate> delegate;

// Returns an input selection UIViewController.
- (UIViewController*)inputSelectionViewController;

// Triggers the secondary transition animation from native LVF to Lens Web.
- (void)triggerSecondaryTransitionAnimation;

@end

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

// Callback invoked when the web load params for a Lens query have been
// generated.
using LensWebParamsCallback =
    base::OnceCallback<void(web::NavigationManager::WebLoadParams)>;

// Returns a controller for the given configuration that can facilitate
// communication with the downstream Lens controller.
id<ChromeLensController> NewChromeLensController(LensConfiguration* config);

// Returns a controller for the given configuration that can facilitate
// communication with the downstream Lens View Finder controller.
UIViewController<ChromeLensViewFinderController>*
NewChromeLensViewFinderController(LensConfiguration* config);

// Returns whether Lens is supported for the current build.
bool IsLensSupported();

// Returns whether or not `url` represents a Lens Web results page.
bool IsLensWebResultsURL(const GURL& url);

// Returns the Lens entry point for `url` if it is a Lens Web results page.
std::optional<LensEntrypoint> GetLensEntryPointFromURL(const GURL& url);

// Generates web load params for a Lens image search for the given
// `query`. `completion` will be run on the main thread.
void GenerateLensLoadParamsAsync(LensQuery* query,
                                 LensWebParamsCallback completion);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
