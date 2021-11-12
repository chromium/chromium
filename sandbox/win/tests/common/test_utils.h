// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
#define SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/win/sid.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  base::win::Sid sid_;
};

// Get the App Container sid for a token.
absl::optional<base::win::Sid> GetTokenAppContainerSid(HANDLE token);

// Get the a list of groups from a token. |information_class| can be one of
// TokenGroups, TokenRestrictedSids or TokenCapabilities.
absl::optional<std::vector<SidAndAttributes>> GetTokenGroups(
    HANDLE token,
    TOKEN_INFORMATION_CLASS information_class);

// Get a variable length property from a token.
absl::optional<std::vector<char>> GetVariableTokenInformation(
    HANDLE token,
    TOKEN_INFORMATION_CLASS information_class);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
