// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_BADGES_SC_BADGE_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_SHOWCASE_BADGES_SC_BADGE_CONTAINER_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"

// Container view controller for badge view controller; this would include some
// control elements other than the badge view controller.
@interface SCBadgeContainerViewController : UIViewController
// Initializer that sets up the badge view controller with |delegate|.
- (instancetype)initWithBadgeDelegate:(id<BadgeDelegate>)delegate;

// The consumer being set up by the showcase badge coordinator.
@property(nonatomic, weak) id<BadgeConsumer> consumer;

@end

#endif  // IOS_SHOWCASE_BADGES_SC_BADGE_CONTAINER_VIEW_CONTROLLER_H_
