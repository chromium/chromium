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
using MiniMapControllerCompletionWithURL = void (^)(NSURL*);

// The completion handler that will be called at the end of the Mini Map flow.
// If the passed String is not nil, it indicates that the address was not
// resolved correctly and must be searched in a new tab.
using MiniMapControllerCompletionWithString = void (^)(NSString*);

@protocol MiniMapController <NSObject>

// Presents the MiniMapController on top of viewController.
- (void)presentMapsWithPresentingViewController:
    (UIViewController*)viewController;

// Presents the MiniMapController in directions mode on top of viewController.
- (void)presentDirectionsWithPresentingViewController:
    (UIViewController*)viewController;

// Configure the footer view of the minimap controller.
// All the fields are required.
// If this is not called before the presentation, no footer view is presented.
- (void)
    configureFooterWithTitle:(NSString*)title
          leadingButtonTitle:(NSString*)leadingButtonTitle
         trailingButtonTitle:(NSString*)trailingButtonTitle
         leadingButtonAction:(void (^)(UIViewController*))leadingButtonAction
        trailingButtonAction:(void (^)(UIViewController*))trailingButtonAction;

// Configure the IPH view of the minimap controller.
// All the fields are required.
// If this is not called before the presentation, no IPH view is presented.
- (void)configureDisclaimerWithTitle:(NSAttributedString*)title
                            subtitle:(NSAttributedString*)subtitle
                       actionHandler:
                           (void (^)(NSURL*, UIViewController*))actionHandler;

@end

namespace ios {
namespace provider {

// Creates a one time MiniMapController to present the mini map for `address`.
// `completion` is called after the minimap is dismissed with an optional URL.
// If present, it indicates that the user requested to open the URL.
id<MiniMapController> CreateMiniMapController(
    NSString* address,
    MiniMapControllerCompletion completion);

// Creates a one time MiniMapController to present the mini map for `address`.
// `completion` is called after the minimap is dismissed with an optional URL.
// If present, it indicates that the user requested to open the URL.
// In case of error, `completion_with_query` is called with a query to open in
// a new tab.
id<MiniMapController> CreateMiniMapController(
    NSString* address,
    MiniMapControllerCompletionWithURL completion,
    MiniMapControllerCompletionWithString completion_with_query);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MINI_MAP_MINI_MAP_API_H_
