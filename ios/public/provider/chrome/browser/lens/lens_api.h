// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_

#import <UIKit/UIKit.h>

@class LensConfiguration;
@class UIViewController;

// A delegate that can receive Lens events forwarded by a ChromeLensController.
@protocol ChromeLensControllerDelegate <NSObject>

// Called when the Lens view controller's dimiss button has been tapped.
- (void)lensControllerDidTapDismissButton;

// Called when a URL in the Lens view controller has been selected.
- (void)lensControllerDidSelectURL:(NSURL*)url;

@end

// A controller that can facilitate communication with the downstream Lens
// controller.
@protocol ChromeLensController <NSObject>

// A delegate that can receive Lens events forwarded by the controller.
@property(nonatomic, weak) id<ChromeLensControllerDelegate> delegate;

// Returns a Lens post-capture view controller for the given query image.
- (UIViewController*)postCaptureViewControllerForImage:(UIImage*)image;

@end

namespace ios {
namespace provider {

// Block invoked when the URL for Lens has been generated. Either
// `url` or `error` is guaranteed to be non-nil.
using LensWebURLCompletion = void (^)(NSURL* url, NSError* error);

// Returns a controller for the given configuration that can facilitate
// communication with the downstream Lens controller.
id<ChromeLensController> NewChromeLensController(LensConfiguration* config);

// Returns whether Lens is supported for the current build.
bool IsLensSupported();

// Generates an URL for a Lens image search. The `completion` will
// be invoked asynchronously in the calling sequence when the url
// has been generated.
void GenerateLensWebURLForImage(UIImage* image,
                                LensWebURLCompletion completion);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_API_H_
