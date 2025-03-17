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
// Contains the user choice for UMA reporting. This value is set to the default
// value when the coordinator is initialized.
@property(nonatomic, assign) BOOL UMAReportingUserChoice;
// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;
// Whether the user tapped on the UMA link.
@property(nonatomic, assign) BOOL UMALinkWasTapped;

// Initializer with 'segmentationService'.
- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
             deviceSwitcherResultDispatcher:
                 (segmentation_platform::DeviceSwitcherResultDispatcher*)
                     dispatcher NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Called when the coordinator is finished.
- (void)finishPresenting;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_MEDIATOR_H_
