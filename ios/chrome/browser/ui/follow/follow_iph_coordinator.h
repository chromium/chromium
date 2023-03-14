// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_IPH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_IPH_COORDINATOR_H_

#import "ios/chrome/browser/follow/follow_iph_presenter.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/follow/follow_iph_presenter.h"

// Coordinator for the Follow IPH feature.
@interface FollowIPHCoordinator : ChromeCoordinator <FollowIPHPresenter>
@end

#endif  // IOS_CHROME_BROWSER_UI_FOLLOW_FOLLOW_IPH_COORDINATOR_H_
