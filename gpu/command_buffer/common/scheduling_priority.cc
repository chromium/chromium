// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/scheduling_priority.h"

#include "base/notreached.h"

namespace gpu {

const char* SchedulingPriorityToString(SchedulingPriority priority) {
  switch (priority) {
    case SchedulingPriority::kHigh:
      return "High";
    case SchedulingPriority::kNormal:
      return "Normal";
    case SchedulingPriority::kLow:
      return "Low";
  }
  NOTREACHED();
  return "";
}

}  // namespace gpu
