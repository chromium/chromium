// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_

@class CrURL;

@protocol AtMemoryCommands <NSObject>

// Commands the parent coordinator to show the AtMemory UI.
- (void)showAtMemory;

// Commands the parent coordinator to dismiss the AtMemory UI.
- (void)dismissAtMemory;

// Commands the coordinator to open the Autofill & Passwords settings page.
- (void)openAutofillSettings;

// TODO(crbug.com/532090671): Remove this optional mark.
@optional
// Commands the coordinator to open a URL.
- (void)openURL:(CrURL*)URL;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_COMMANDS_H_
