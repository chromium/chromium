// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_TEST_UTILS_H_

#import <string>

#import "components/signin/public/identity_manager/account_info.h"

class ProfileIOS;

namespace gemini::test {

// Sets up a signed-in primary account with Gemini capabilities for `profile`.
AccountInfo SetUpEligibleAccount(ProfileIOS* profile,
                                 const std::string& email = "test@example.com",
                                 bool can_use_model_execution = true,
                                 bool can_use_gemini_in_chrome = true);

}  // namespace gemini::test

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_TEST_UTILS_H_
