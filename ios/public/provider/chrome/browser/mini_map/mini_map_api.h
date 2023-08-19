// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MINI_MAP_MINI_MAP_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MINI_MAP_MINI_MAP_API_H_

#import <UIKit/UIKit.h>

// The completion handler that will be called at the end of the Mini Map flow.
// If the passed URL is not nil, it indicates that the user requested to open
// this URL.
using MiniMapControllerCompletion = void (^)(NSURL*);

@protocol MiniMapController <NSObject>

// Presents the MiniMapController on top of viewController.
- (void)presentMapsWithPresentingViewController:
    (UIViewController*)viewController;

// Presents the MiniMapController in directions mode on top of viewController.
- (void)presentDirectionsWithPresentingViewController:
    (UIViewController*)viewController;

@end

namespace ios {
namespace provider {

// Creates a one time MiniMapController to present the mini map for `address`.
// `completion` is called after the minimap is dismissed with an optional URL.
// If present, it indicates that the user requested to open the URL.
id<MiniMapController> CreateMiniMapController(
    NSString* address,
    MiniMapControllerCompletion completion);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_API_H_
