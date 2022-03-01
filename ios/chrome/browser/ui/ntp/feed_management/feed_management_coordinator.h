// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// The top-level owner of the Feed Management component. It serves to connect
// the various independent pieces such as the Feed Management UI, the Follow
// Management UI, and mediators.
@interface FeedManagementCoordinator : ChromeCoordinator
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_COORDINATOR_H_
