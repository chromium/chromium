// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DATA_CONTROLS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DATA_CONTROLS_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "components/enterprise/data_controls/core/browser/data_controls_dialog.h"

// Commands for displaying Data Controls dialogs.
@protocol DataControlsCommands <NSObject>

// Commands to show a warning dialog to warn user that the copy/paste actions
// may violate their organization's policy.
- (void)showDataControlsWarningDialog:
            (data_controls::DataControlsDialog::Type)dialogType
                   organizationDomain:(std::string_view)organizationDomain
                             callback:(base::OnceCallback<void(bool)>)callback;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DATA_CONTROLS_COMMANDS_H_
