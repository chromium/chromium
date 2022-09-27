// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// https://wicg.github.io/scheduling-apis/#sec-task-priorities
enum class WebSchedulingPriority {
  kUserBlockingPriority = 0,
  kUserVisiblePriority = 1,
  kBackgroundPriority = 2,

  kLastPriority = kBackgroundPriority
};

PLATFORM_EXPORT AtomicString
    WebSchedulingPriorityToString(WebSchedulingPriority);
PLATFORM_EXPORT WebSchedulingPriority
WebSchedulingPriorityFromString(const AtomicString&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_WEB_SCHEDULING_PRIORITY_H_
