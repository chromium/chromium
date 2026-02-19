// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_ENTERPRISE_COMMANDS_HANDLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_ENTERPRISE_COMMANDS_HANDLER_H_

#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"

// Fake commands handler for sending EnterpriseDialog.
@interface FakeEnterpriseCommandsHandler : NSObject <EnterpriseCommands> {
 @public
  base::OnceCallback<void(bool)> _callback;
}
@property(readonly, nonatomic) enterprise::DialogType dialogType;
@property(readonly, nonatomic) std::string organizationDomain;
@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_ENTERPRISE_COMMANDS_HANDLER_H_
