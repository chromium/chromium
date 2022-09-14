// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/navigation/navigation_manager.h"

@class LensConfiguration;
@class UIViewController;
enum class LensEntrypoint;

// A delegate that can receive Lens events forwarded by a ChromeLensController.
@protocol ChromeLensControllerDelegate <NSObject>

// Called when the Lens view controller's dimiss button has been tapped.
- (void)lensControllerDidTapDismissButton;

// Called when the user selects an image and the Lens controller has prepared
// `params` for loading a Lens web page.
- (void)lensControllerDidGenerateLoadParams:
    (const web::NavigationManager::WebLoadParams&)params;

@end

// A controller that can facilitate communication with the downstream Lens
// controller.
@protocol ChromeLensController <NSObject>

// A delegate that can receive Lens events forwarded by the controller.
@property(nonatomic, weak) id<ChromeLensControllerDelegate> delegate;

// Returns an input selection UIViewController with the provided
// web content frame.
- (UIViewController*)inputSelectionViewControllerWithWebContentFrame:
    (CGRect)webContentFrame;

@end

namespace ios {
namespace provider {

// Returns a controller for the given configuration that can facilitate
// communication with the downstream Lens controller.
id<ChromeLensController> NewChromeLensController(LensConfiguration* config);

// Returns whether Lens is supported for the current build.
bool IsLensSupported();

// Generates web load params for a Lens image search for the given
// 'image' and 'entry_point'.
web::NavigationManager::WebLoadParams GenerateLensLoadParamsForImage(
    UIImage* image,
    LensEntrypoint entry_point,
    bool is_incognito);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
