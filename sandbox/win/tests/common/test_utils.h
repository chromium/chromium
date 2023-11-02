// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
#define SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_

#include "base/win/windows_types.h"

namespace sandbox {

// Sets a reparse point. |source| will now point to |target|. Returns true if
// the call succeeds, false otherwise.
bool SetReparsePoint(HANDLE source, const wchar_t* target);

// Delete the reparse point referenced by |source|. Returns true if the call
// succeeds, false otherwise.
bool DeleteReparsePoint(HANDLE source);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_TEST_UTILS_H_
