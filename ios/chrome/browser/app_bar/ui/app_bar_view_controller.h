// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_consumer.h"

@protocol AppBarMutator;
@class LayoutGuideCenter;
@protocol SceneCommands;
@protocol TabGridCommands;

// View controller for the app bar.
@interface AppBarViewController : UIViewController <AppBarConsumer>

// The mutator.
@property(nonatomic, weak) id<AppBarMutator> mutator;
// This view controller's LayoutGuideCenter.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
// Command handler for the Scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;
// Tab Grid handler.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// Updates the App Bar's subviews for a given rotation angle.
- (void)updateForAngle:(CGFloat)angle;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_VIEW_CONTROLLER_H_
