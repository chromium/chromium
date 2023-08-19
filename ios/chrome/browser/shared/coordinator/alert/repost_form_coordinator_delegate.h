// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_DELEGATE_H_

@class RepostFormCoordinator;

// Delegate for the report form coordinator.
@protocol RepostFormCoordinatorDelegate <NSObject>

// Requests the delegate to stop the coordinator.
- (void)repostFormCoordinatorWantsToBeDismissed:
    (RepostFormCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_DELEGATE_H_
