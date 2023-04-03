// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COMMANDS_H_

// Keep in sync with PasswordManager.AccountStorageNoticeDismissalReason
// suffixes in tools/metrics/histograms/metadata/password/histograms.xml.
enum class PasswordsAccountStorageNoticeEntryPoint {
  kFill,
  kSave,
  kUpdate,
};

// Commands for the bottom sheet that notifies the user they are now saving
// passwords to their Google Account.
@protocol PasswordsAccountStorageNoticeCommands

// Asks the UI to show the passwords account storage notice. This method must
// not be called if the notice is already being shown. `dismissalHandler` must
// be non-nil and called once the UI dismissed all its UIViewControllers.
- (void)showPasswordsAccountStorageNoticeForEntryPoint:
            (PasswordsAccountStorageNoticeEntryPoint)entryPoint
                                      dismissalHandler:
                                          (void (^)())dismissalHandler;

// Asks the UI to hide the passwords account storage notice. This method may be
// called multiple times and is expected to silently no-op if the notice isn't
// being shown.
- (void)hidePasswordsAccountStorageNotice;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PASSWORDS_ACCOUNT_STORAGE_NOTICE_COMMANDS_H_
