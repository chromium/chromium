// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

namespace commerce {
class ShoppingService;
}

namespace segmentation_platform {
class SegmentationPlatformService;
}

@protocol BestFeaturesScreenConsumer;

// Mediator for the Best Feature's Screen.
@interface BestFeaturesScreenMediator : NSObject

// The consumer for this object. Setting it will update the consumer with
// current data.
@property(nonatomic, weak) id<BestFeaturesScreenConsumer> consumer;

// Designated initializer for this mediator.
- (instancetype)
    initWithSegmentationService:
        (segmentation_platform::SegmentationPlatformService*)segmentationService
                shoppingService:(commerce::ShoppingService*)shoppingService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Retrieves the status of the user's Shopping segment.
- (void)retrieveShoppingUserSegmentWithCompletion:(ProceduralBlock)completion;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_COORDINATOR_BEST_FEATURES_SCREEN_MEDIATOR_H_
