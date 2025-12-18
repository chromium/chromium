// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OMNIBOX_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OMNIBOX_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// Commands for focusing the omnibox in various ways.
@protocol OmniboxCommands
// Give focus to the omnibox, if it is visible. No-op if it is not visible.
// If current page is an NTP, first focus the NTP fakebox.
- (void)focusOmnibox;
// Focus the omnibox but skip the NTP check.
- (void)focusOmniboxFromFakebox;
// Moves the focus of VoiceOver to the omnibox, without activating it.
- (void)focusOmniboxForVoiceOver;
// Cancel omnibox edit (from shield tap or cancel button tap).
- (void)cancelOmniboxEdit;
// Cancels the omnibox edit session. The completion block is executed
// once the cancellation (and any dismissal animation) has finished.
- (void)cancelOmniboxEditWithCompletion:(ProceduralBlock)completion;
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OMNIBOX_COMMANDS_H_
