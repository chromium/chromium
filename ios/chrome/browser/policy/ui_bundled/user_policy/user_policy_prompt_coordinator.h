// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol UserPolicyPromptCoordinatorDelegate;

@interface UserPolicyPromptCoordinator : ChromeCoordinator

// Delegate for the coordinator. Can be a parent coordinator that owns this
// coordinator or a scene agent.
@property(nonatomic, weak) id<UserPolicyPromptCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_H_
