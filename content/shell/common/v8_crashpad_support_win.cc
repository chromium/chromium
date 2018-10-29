// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/v8_crashpad_support_win.h"

#include <windows.h>

#include "base/logging.h"
#include "gin/public/debug.h"

namespace v8_crashpad_support {

void SetUp() {
#ifdef _WIN64
  // Get the breakpad pointer from content_shell.exe
  gin::Debug::CodeRangeCreatedCallback create_callback =
      reinterpret_cast<gin::Debug::CodeRangeCreatedCallback>(
          ::GetProcAddress(::GetModuleHandle(L"content_shell.exe"),
                           "RegisterNonABICompliantCodeRange"));
  gin::Debug::CodeRangeDeletedCallback delete_callback =
      reinterpret_cast<gin::Debug::CodeRangeDeletedCallback>(
          ::GetProcAddress(::GetModuleHandle(L"content_shell.exe"),
                           "UnregisterNonABICompliantCodeRange"));
  if (create_callback && delete_callback) {
    gin::Debug::SetCodeRangeCreatedCallback(create_callback);
    gin::Debug::SetCodeRangeDeletedCallback(delete_callback);
  }
#endif
}

}  // namespace v8_crashpad_support
