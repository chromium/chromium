// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
class SegmentationPlatformService;
}  // namespace segmentation_platform

@protocol DefaultBrowserGenericPromoConsumer;

// The mediator for the generic default browser promo, with personalized
// messaging for users identified by the Segmentation Platform as being in a
// targeted segment.
@interface DefaultBrowserGenericPromoMediator : NSObject

// Main consumer for this mediator.
@property(nonatomic, weak) id<DefaultBrowserGenericPromoConsumer> consumer;

// Initializer with `segmentationService` and `deviceSwitcherResultDispatcher`.
- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher;

// Handles user tap on primary action. Allows the user to open the iOS settings.
- (void)didTapPrimaryActionButton;

// Sets the user segment retrieved from the Segmentation Platform.
- (void)retrieveUserSegmentWithCompletion:(ProceduralBlock)completion;

// Disconnects this mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_MEDIATOR_H_
