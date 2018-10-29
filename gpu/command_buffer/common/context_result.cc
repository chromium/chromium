// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/context_result.h"

namespace gpu {

bool IsFatalOrSurfaceFailure(ContextResult result) {
  return result == ContextResult::kFatalFailure ||
         result == ContextResult::kSurfaceFailure;
}

}  // namespace gpu
