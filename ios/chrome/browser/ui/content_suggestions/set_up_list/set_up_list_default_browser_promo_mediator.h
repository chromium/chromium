// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

namespace segmentation_platform {
class SegmentationPlatformService;
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

@protocol DefaultBrowserScreenConsumer;

// Mediator for presenting segmented default browser promo.
@interface SetUpListDefaultBrowserPromoMediator : NSObject

// Main consumer for this mediator.
@property(nonatomic, weak) id<DefaultBrowserScreenConsumer> consumer;

// Initializer with 'segmentationService' and `deviceSwitcherResultDispatcher`
// for retrieving segmentation info from Segmentation Platform.
- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Sets the user segment retrieved from the Segmentation Platform.
- (void)retrieveUserSegmentWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_DEFAULT_BROWSER_PROMO_MEDIATOR_H_
