// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FAKE_CREDENTIAL_PROVIDER_SETTINGS_HANDLER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FAKE_CREDENTIAL_PROVIDER_SETTINGS_HANDLER_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

@class CommandDispatcher;
@protocol SettingsCommands;

// Test helper that records `-showPasswordDetailsForCredential:inEditMode:`
// calls forwarded by `CommandDispatcher`, while rejecting all other
// `SettingsCommands` selectors via an internal strict protocol mock (`OCMock`'s
// `NSInvocation` forwarding cannot handle non-trivial C++ `CredentialUIEntry`
// structs passed by value).
@interface FakeCredentialProviderSettingsHandler : NSObject

// Number of `-showPasswordDetailsForCredential:inEditMode:` invocations.
@property(nonatomic, readonly) NSUInteger showPasswordDetailsCallCount;

// `editMode` argument from the most recent invocation.
@property(nonatomic, readonly, getter=isCalledInEditMode) BOOL calledInEditMode;

// `credential` argument from the most recent invocation, or `std::nullopt` if
// never invoked.
@property(nonatomic, readonly)
    std::optional<password_manager::CredentialUIEntry>
        lastCredential;

// Registers a strict `SettingsCommands` mock (retained by `self`) on
// `dispatcher` and routes `-showPasswordDetailsForCredential:inEditMode:` to
// `self`. Returns the typed `SettingsCommands` handler for `dispatcher`.
- (id<SettingsCommands>)registerWithCommandDispatcher:
    (CommandDispatcher*)dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_FAKE_CREDENTIAL_PROVIDER_SETTINGS_HANDLER_H_
