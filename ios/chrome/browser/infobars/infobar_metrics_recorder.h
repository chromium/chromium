// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/infobars/infobar_type.h"

// Values for the UMA Mobile.Messages.Banner.Event histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MobileMessagesBannerEvent {
  // Infobar Banner was accepted.
  Accepted = 0,
  // Infobar Banner was handled by a gesture.
  Handled = 1,
  // Infobar Banner was dismissed.
  Dismissed = 2,
  // Infobar Banner was presented.
  Presented = 3,
  // Infobar Banner returned to its origin after a gesture.
  ReturnedToOrigin = 4,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = ReturnedToOrigin,
};

// Values for the UMA Mobile.Messages.Banner.Dismiss histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MobileMessagesBannerDismissType {
  // Infobar Banner was dismissed due to a time out.
  TimedOut = 0,
  // Infobar Banner was dismissed by being swiped up.
  SwipedUp = 1,
  // *DEPRECATED* Infobar Banner was dismissed by being dragged into an Infobar
  // Modal.
  ExpandedToModal_DEPRECATED = 2,
  // Infobar Banner was dismissed by being tapped into an Infobar Modal.
  TappedToModal = 3,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = TappedToModal,
};

// Values for the UMA Mobile.Messages.Modal.Event histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MobileMessagesModalEvent {
  // Infobar Modal was accepted.
  Accepted = 0,
  // Infobar Modal was canceled.
  Canceled = 1,
  // Infobar Modal was dismissed.
  Dismissed = 2,
  // Infobar Modal was presented.
  Presented = 3,
  // Infobar Modal settings were opened.
  SettingsOpened = 4,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = SettingsOpened,
};

// Values for the UMA Mobile.Messages.Badge.Tapped histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MobileMessagesBadgeState {
  // Infobar Badge is inactive.
  Inactive = 0,
  // Infobar Badge is active.
  Active = 1,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = Active,
};

// Used to record metrics related to Infobar events.
@interface InfobarMetricsRecorder : NSObject

// Designated initializer. InfobarMetricsRecorder will record metrics for
// |infobarType|.
- (instancetype)initWithType:(InfobarType)infobarType NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Records histogram for Banner |event|.
- (void)recordBannerEvent:(MobileMessagesBannerEvent)event;

// Records histogram for Banner |dismissType|.
- (void)recordBannerDismissType:(MobileMessagesBannerDismissType)dismissType;

// Records histogram for Banner On Screen duration.
- (void)recordBannerOnScreenDuration:(double)duration;

// Records histogram for Modal |event|.
- (void)recordModalEvent:(MobileMessagesModalEvent)event;

// Records histogram for Badge Tapped in |state|.
- (void)recordBadgeTappedInState:(MobileMessagesBadgeState)state;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_METRICS_RECORDER_H_
