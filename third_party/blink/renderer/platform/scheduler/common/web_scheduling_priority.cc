// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

#include "base/notreached.h"

namespace blink {

namespace {

const char kUserBlockingPriorityKeyword[] = "user-blocking";
const char kUserVisiblePriorityKeyword[] = "user-visible";
const char kBackgroundPriorityKeyword[] = "background";

}  // namespace

AtomicString WebSchedulingPriorityToString(WebSchedulingPriority priority) {
  switch (priority) {
    case WebSchedulingPriority::kUserBlockingPriority:
      return AtomicString(kUserBlockingPriorityKeyword);
    case WebSchedulingPriority::kUserVisiblePriority:
      return AtomicString(kUserVisiblePriorityKeyword);
    case WebSchedulingPriority::kBackgroundPriority:
      return AtomicString(kBackgroundPriorityKeyword);
  }

  NOTREACHED();
  return g_empty_atom;
}

WebSchedulingPriority WebSchedulingPriorityFromString(
    const AtomicString& priority) {
  if (priority == kUserBlockingPriorityKeyword)
    return WebSchedulingPriority::kUserBlockingPriority;
  if (priority == kUserVisiblePriorityKeyword)
    return WebSchedulingPriority::kUserVisiblePriority;
  if (priority == kBackgroundPriorityKeyword)
    return WebSchedulingPriority::kBackgroundPriority;
  NOTREACHED();
  return WebSchedulingPriority::kUserVisiblePriority;
}

}  // namespace blink
