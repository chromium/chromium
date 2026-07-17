// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_BREAKOUT_BOX_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_BREAKOUT_BOX_UTIL_H_

#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class ExecutionContext;
class Performance;

MODULES_EXPORT Performance* GetPerformanceFromExecutionContext(
    ExecutionContext* context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_BREAKOUT_BOX_UTIL_H_
