// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_BREACH_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_BREACH_COMMANDS_H_

#import <UIKit/UIKit.h>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

class GURL;

using password_manager::CredentialLeakType;

// Commands related to Password Breach.
@protocol PasswordBreachCommands

// Shows Password Breach for |leakType| and |URL|.
- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_BREACH_COMMANDS_H_
