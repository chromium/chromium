// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_INTERNAL_TYPES_H_
#define SANDBOX_WIN_SRC_INTERNAL_TYPES_H_

#include <stdint.h>

#include "base/memory/raw_span.h"

namespace sandbox {

const wchar_t kNtdllName[] = L"ntdll.dll";
const wchar_t kKerneldllName[] = L"kernel32.dll";

// Defines the supported C++ types encoding to numeric id. Like a simplified
// RTTI. Note that true C++ RTTI will not work because the types are not
// polymorphic anyway.
enum ArgType {
  INVALID_TYPE = 0,
  WCHAR_TYPE,
  UINT32_TYPE,
  VOIDPTR_TYPE,
  INOUTPTR_TYPE,
  LAST_TYPE
};

// Encapsulates a pointer to a buffer and the size of the buffer.
using CountedBuffer = base::raw_span<uint8_t>;

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_INTERNAL_TYPES_H_
