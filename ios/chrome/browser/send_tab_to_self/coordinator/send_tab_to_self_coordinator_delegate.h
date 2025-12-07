// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_DELEGATE_H_

@class SendTabToSelfCoordinator;

@protocol SendTabToSelfCoordinatorDelegate <NSObject>

// Requests the delegate to stop `coordinator`
- (void)sendTabToSelfCoordinatorWantsToBeStopped:
    (SendTabToSelfCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_COORDINATOR_SEND_TAB_TO_SELF_COORDINATOR_DELEGATE_H_
