// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_nt_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size % 2 == 1)
    return 0;

  std::wstring_view module_path{
      reinterpret_cast<wchar_t*>(const_cast<uint8_t*>(data)),
      size / sizeof(wchar_t)};
  sandbox::ExtractModuleName(module_path);
  return 0;
}
