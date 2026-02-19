// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENTERPRISE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENTERPRISE_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"

// Commands for displaying Enterprise dialogs.
@protocol EnterpriseCommands <NSObject>

// Commands to show a warning dialog to warn user that their actions may violate
// their organization's policy.
- (void)showEnterpriseWarningDialog:(enterprise::DialogType)dialogType
                 organizationDomain:(std::string_view)organizationDomain
                           callback:(base::OnceCallback<void(bool)>)callback;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ENTERPRISE_COMMANDS_H_
