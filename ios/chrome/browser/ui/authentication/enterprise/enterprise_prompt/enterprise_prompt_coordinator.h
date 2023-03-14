// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_type.h"

// Delegate for the coordinator.
@protocol EnterprisePromptCoordinatorDelegate

// Command to clean up the prompt. Stops the coordinator and sets it to
// nil. `learnMore` is YES if the user tapped the "learn more" button.
- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore;

@end

// Coordinator for enterprise prompt alerts.
@interface EnterprisePromptCoordinator : ChromeCoordinator

// Initializes this Coordinator with its `browser` and the `promptType`.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                promptType:(EnterprisePromptType)promptType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<EnterprisePromptCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_PROMPT_ENTERPRISE_PROMPT_COORDINATOR_H_
