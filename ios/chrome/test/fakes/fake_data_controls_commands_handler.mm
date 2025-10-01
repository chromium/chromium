// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_data_controls_commands_handler.h"

@implementation FakeDataControlsCommandsHandler
@synthesize dialogType = _dialogType;

- (void)showDataControlsWarningDialog:
            (data_controls::DataControlsDialog::Type)dialogType
                             callback:(base::OnceCallback<void(bool)>)callback {
  _dialogType = dialogType;
  _callback = std::move(callback);
}

@end
