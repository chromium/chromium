// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BLOCKING_SCENE_COMMANDS_H_
#define IOS_CHROME_APP_BLOCKING_SCENE_COMMANDS_H_

#import <UIKit/UIKit.h>

// App-level commands related to blocking UI, such as First Run.
@protocol BlockingSceneCommands

// Activates the scene that currently shows blocking UI.
- (void)activateBlockingScene:(UIScene*)requestingScene;

@end

#endif  // IOS_CHROME_APP_BLOCKING_SCENE_COMMANDS_H_
