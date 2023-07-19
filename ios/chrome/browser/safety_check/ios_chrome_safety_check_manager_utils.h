// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_

#import <vector>

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

enum class PasswordCheckState;
enum class PasswordSafetyCheckState;

// Returns the Password check state (Safety Check version) given a
// `check_state`, insecure credentials list, and previous Password check state
// (Safety Check version).
PasswordSafetyCheckState CalculatePasswordSafetyCheckState(
    PasswordCheckState check_state,
    const std::vector<password_manager::CredentialUIEntry>&
        insecure_credentials,
    PasswordSafetyCheckState previous_check_state);

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_IOS_CHROME_SAFETY_CHECK_MANAGER_UTILS_H_
