// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/v8_crashpad_support_win.h"

#include <windows.h>
#include "components/crash/core/app/crash_export_thunks.h"
#include "gin/public/debug.h"

namespace v8_crashpad_support {

void SetUp() {
#ifdef _WIN64
  gin::Debug::SetUnhandledExceptionCallback(&CrashForException_ExportThunk);
#endif
}

}  // namespace v8_crashpad_support
