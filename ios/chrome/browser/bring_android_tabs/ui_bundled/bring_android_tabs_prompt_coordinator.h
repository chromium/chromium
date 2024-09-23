// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import <UIKit/UIKit.h>

@protocol BringAndroidTabsCommands;

// Coordinator that manages the "Bring Android Tabs" prompt presentation and
// interaction.
@interface BringAndroidTabsPromptCoordinator : ChromeCoordinator

// View controller for the prompt.
@property(nonatomic, readonly) UIViewController* viewController;
// Command handler for the prompt.
@property(nonatomic, weak) id<BringAndroidTabsCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_COORDINATOR_H_
