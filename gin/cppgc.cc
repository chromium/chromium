// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/cppgc.h"
#include "gin/public/v8_platform.h"
#include "v8/include/cppgc/platform.h"

namespace gin {

void InitializeCppgcFromV8Platform() {
  static bool cppgc_is_initialized = false;
  if (cppgc_is_initialized)
    return;

  cppgc::InitializeProcess(gin::V8Platform::Get()->GetPageAllocator());

  cppgc_is_initialized = true;
}

}  // namespace gin
