// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_THUMB_STRIP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_THUMB_STRIP_COMMANDS_H_

// Commands for manipulating the state of the thumb strip.
@protocol ThumbStripCommands
enum class ViewRevealTrigger;

// Asks the thumb strip to close itself. This may happen after some delay if the
// thumb strip is already transitioninig.
- (void)closeThumbStripWithTrigger:(ViewRevealTrigger)trigger;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_THUMB_STRIP_COMMANDS_H_
