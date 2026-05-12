// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"

@protocol LevelUpCommands;

// View controller displaying Level Up bottom sheet.
@interface LevelUpViewController : UIViewController <LevelUpConsumer>

// Command handler for Level Up commands.
@property(nonatomic, weak) id<LevelUpCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_VIEW_CONTROLLER_H_
