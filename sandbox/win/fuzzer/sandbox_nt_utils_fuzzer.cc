// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

using ScopedUnicodeString =
    std::unique_ptr<UNICODE_STRING, sandbox::NtAllocDeleter>;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size % 2 == 1)
    return 0;

  UNICODE_STRING module_path;
  module_path.Buffer = reinterpret_cast<wchar_t*>(const_cast<uint8_t*>(data));
  module_path.Length = size;
  module_path.MaximumLength = size;

  ScopedUnicodeString result(sandbox::ExtractModuleName(&module_path));
  return 0;
}
