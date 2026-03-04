// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_

#import <Foundation/Foundation.h>

#import <string>

// Returns the deletion confirmation message based on
// `profile_count` and if the selection has local, account or home/work
// profiles.
// This is used before passes are added into "Addresses and more".
// The message it returns may contain "address" or "addresses" depending on the
// profile count.
NSString* GetDeletionConfirmationString(int profile_count,
                                        bool has_local_profile,
                                        bool has_account_profile,
                                        bool has_home_work_name_email_profile,
                                        const std::u16string& user_email);

// Returns the deletion confirmation message when Autofill AI entities
// are selected for deletion. This is a simplified version of
// `GetDeletionConfirmationString` with just two strings. It uses "info" instead
// of "address" or "addresses" in the message.
// `has_server_data` is true if the selection contains server data. Autofill AI
// entities are only allowed to delete when it is a local entity.
// So, here `has_server_data` means any account profile, or home/work profile.
NSString* GetDeletionConfirmationStringWithEntities(
    bool has_server_data,
    const std::u16string& user_email);

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_UTILS_AUTOFILL_SETTINGS_UI_UTIL_H_
