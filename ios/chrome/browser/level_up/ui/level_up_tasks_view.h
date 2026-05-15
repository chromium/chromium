// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASKS_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASKS_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"

// View displaying the card list of available Chrome level-up tasks.
@interface LevelUpTasksView : UIView <LevelUpConsumer>

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASKS_VIEW_H_
