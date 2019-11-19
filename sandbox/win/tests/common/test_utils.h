// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
#define SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "sandbox/win/src/sid.h"

namespace sandbox {

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const wchar_t* target);

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source);

// Class to hold a single Sid with attributes for a group list.
class SidAndAttributes {
 public:
  SidAndAttributes(const SID_AND_ATTRIBUTES& sid_and_attributes);

  PSID GetPSID() const;
  DWORD GetAttributes() const;

 private:
  DWORD attributes_;
  sandbox::Sid sid_;
};

// Get the App Container sid for a token.
bool GetTokenAppContainerSid(HANDLE token,
                             std::unique_ptr<sandbox::Sid>* app_container_sid);

// Get the a list of groups from a token. |information_class| can be one of
// TokenGroups, TokenRestrictedSids or TokenCapabilites.
bool GetTokenGroups(HANDLE token,
                    TOKEN_INFORMATION_CLASS information_class,
                    std::vector<SidAndAttributes>* groups);

// Get a variable length property from a token.
bool GetVariableTokenInformation(HANDLE token,
                                 TOKEN_INFORMATION_CLASS information_class,
                                 std::vector<char>* information);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
