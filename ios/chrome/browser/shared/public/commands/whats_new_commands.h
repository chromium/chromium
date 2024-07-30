// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_

// Commands related to What's new.
@protocol WhatsNewCommands

// Shows what's new.
- (void)showWhatsNew;

// Dismisses what's new.
- (void)dismissWhatsNew;

// Shows what's new IPH.
- (void)showWhatsNewIPH;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_WHATS_NEW_COMMANDS_H_
