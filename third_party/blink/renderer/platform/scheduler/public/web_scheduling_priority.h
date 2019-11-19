// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// Priorities for the experimental scheduling API (see
// https://github.com/WICG/main-thread-scheduling).
enum class WebSchedulingPriority {
  kImmediatePriority = 0,
  kHighPriority = 1,
  kDefaultPriority = 2,
  kLowPriority = 3,
  kIdlePriority = 4,

  kLastPriority = kIdlePriority
};

PLATFORM_EXPORT AtomicString
    WebSchedulingPriorityToString(WebSchedulingPriority);
PLATFORM_EXPORT WebSchedulingPriority
WebSchedulingPriorityFromString(const AtomicString&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_
