// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_DATA_CONTROLS_COMMANDS_HANDLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_DATA_CONTROLS_COMMANDS_HANDLER_H_

#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"

// Fake commands handler for DataControlsTabHelper.
@interface FakeDataControlsCommandsHandler : NSObject <DataControlsCommands> {
 @public
  base::OnceCallback<void(bool)> _callback;
}
@property(readonly, nonatomic)
    data_controls::DataControlsDialog::Type dialogType;
@property(readonly, nonatomic) std::string organizationDomain;
@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_DATA_CONTROLS_COMMANDS_HANDLER_H_
