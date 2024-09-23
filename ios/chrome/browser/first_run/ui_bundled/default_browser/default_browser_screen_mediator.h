// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace segmentation_platform {
class SegmentationPlatformService;
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

@protocol DefaultBrowserScreenConsumer;

// Mediator for presenting segmented default browser promo.
@interface DefaultBrowserScreenMediator : NSObject

// Main consumer for this mediator.
@property(nonatomic, weak) id<DefaultBrowserScreenConsumer> consumer;

// Initializer with 'segmentationService'.
- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher;

// Disconnects this mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
