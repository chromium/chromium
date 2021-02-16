// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to Password Protection.
@protocol PasswordProtectionCommands

// Shows the Password Protection warning with |warningText|.
- (void)showPasswordProtectionWarning:(NSString*)warningText;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_
