// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/fake_credential_provider_settings_handler.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@implementation FakeCredentialProviderSettingsHandler {
  id<SettingsCommands> _strictSettingsMock;
}

#pragma mark - Public

- (id<SettingsCommands>)registerWithCommandDispatcher:
    (CommandDispatcher*)dispatcher {
  CHECK(dispatcher);
  _strictSettingsMock = OCMStrictProtocolMock(@protocol(SettingsCommands));
  [dispatcher startDispatchingToTarget:_strictSettingsMock
                           forProtocol:@protocol(SettingsCommands)];
  [dispatcher stopDispatchingForSelector:
                  @selector(showPasswordDetailsForCredential:inEditMode:)];
  [dispatcher
      startDispatchingToTarget:self
                   forSelector:
                       @selector(showPasswordDetailsForCredential:inEditMode:)];
  return HandlerForProtocol(dispatcher, SettingsCommands);
}

#pragma mark - SettingsCommands

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  ++_showPasswordDetailsCallCount;
  _lastCredential = std::move(credential);
  _calledInEditMode = editMode;
}

@end
