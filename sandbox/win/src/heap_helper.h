// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_HEAP_HELPER_H_
#define SANDBOX_WIN_SRC_HEAP_HELPER_H_

#include "base/win/windows_types.h"

namespace sandbox {
// These helper functions are not expected to be used generally, but are exposed
// only to allow direct testing of this functionality.

// Return the flags for this heap handle. Limited verification of the handle is
// performed. No verification of the flags is performed.
bool HeapFlags(HANDLE handle, DWORD* flags);

// Return the handle to the CSR Port Heap, return nullptr if none or more than
// one candidate was found.
HANDLE FindCsrPortHeap();

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_HEAP_HELPER_H_
