// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MENU_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"

// An enum of all the Feed Menu item types.
enum class FeedMenuItemType {
  kCancel,
  kTurnOff,
  kTurnOn,
  kManage,
  kManageActivity,
  kManageInterests,
  kLearnMore,

  kMaxValue = kLearnMore,
};

// Defines callbacks that the FeedMenuCoordinator will send.
@protocol FeedMenuCoordinatorDelegate

// Called when a Feed Menu item is selected by the user.
- (void)didSelectFeedMenuItem:(FeedMenuItemType)item;

@end

// A coordinator which can display the Feed Menu and notify the delegate when
// an item is selected.
@interface FeedMenuCoordinator : ChromeCoordinator <FeedMenuCommands>

// A delegate to receive callbacks when a Feed Menu item is selected.
@property(nonatomic, weak) id<FeedMenuCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MENU_COORDINATOR_H_
