// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_COMMANDS_H_
#define IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_COMMANDS_H_

#import <Foundation/Foundation.h>

@class AssistantBarConfiguration;

// Commands to control the Assistant UI.
@protocol AssistantCommands <NSObject>

// Updates the bar configuration.
- (void)updateBarConfiguration:(AssistantBarConfiguration*)configuration;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_COORDINATOR_ASSISTANT_COMMANDS_H_
