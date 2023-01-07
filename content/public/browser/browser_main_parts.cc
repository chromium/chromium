// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_main_parts.h"

#include "content/public/common/result_codes.h"

namespace content {

int BrowserMainParts::PreEarlyInitialization() {
  return RESULT_CODE_NORMAL_EXIT;
}

int BrowserMainParts::PreCreateThreads() {
  return RESULT_CODE_NORMAL_EXIT;
}

int BrowserMainParts::PreMainMessageLoopRun() {
  return RESULT_CODE_NORMAL_EXIT;
}

bool BrowserMainParts::ShouldInterceptMainMessageLoopRun() {
  return true;
}

}  // namespace content
