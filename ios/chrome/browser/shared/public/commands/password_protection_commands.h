// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace safe_browsing {
enum class WarningAction;
}

// Commands related to Password Protection.
@protocol PasswordProtectionCommands

// Shows the Password Protection warning with `warningText`. `completion` should
// be called when the warning is dismissed with the user's `action`.
- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORD_PROTECTION_COMMANDS_H_
