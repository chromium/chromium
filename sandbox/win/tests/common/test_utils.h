// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
#define SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_

#include <optional>
#include "base/win/access_control_list.h"
#include "base/win/windows_types.h"

namespace sandbox {

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const wchar_t* target);

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source);

// Check if a specific SID and access mask is presenting in a DACL.
// `dacl` is the DACL to check.
// `allowed` if true checks for access allowed ACEs, otherwise for deny ACEs.
// `mask` check for a specific access mask. Ignored if the value is empty.
// `sid` the SID to check for.
// Returns true if the SID and access mask is present.
bool IsSidInDacl(const base::win::AccessControlList& dacl,
                 bool allowed,
                 std::optional<ACCESS_MASK> mask,
                 const base::win::Sid& sid);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
