// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_

// Protocol that describes the commands that trigger Toolbar UI changes.
@protocol ToolbarCommands

// Triggers a slide-in animation for the toolbar(s).
- (void)triggerToolbarSlideInAnimation;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TOOLBAR_COMMANDS_H_
