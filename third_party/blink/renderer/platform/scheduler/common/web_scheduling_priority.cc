// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

#include "base/notreached.h"

namespace blink {

namespace {

const AtomicString& UserBlockingPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, user_blocking_priority,
                      ("user-blocking"));
  return user_blocking_priority;
}

const AtomicString& UserVisiblePriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, user_visible_priority,
                      ("user-visible"));
  return user_visible_priority;
}

const AtomicString& BackgroundPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, background_priority, ("background"));
  return background_priority;
}

}  // namespace

AtomicString WebSchedulingPriorityToString(WebSchedulingPriority priority) {
  switch (priority) {
    case WebSchedulingPriority::kUserBlockingPriority:
      return UserBlockingPriorityKeyword();
    case WebSchedulingPriority::kUserVisiblePriority:
      return UserVisiblePriorityKeyword();
    case WebSchedulingPriority::kBackgroundPriority:
      return BackgroundPriorityKeyword();
  }

  NOTREACHED();
  return g_empty_atom;
}

WebSchedulingPriority WebSchedulingPriorityFromString(
    const AtomicString& priority) {
  if (priority == UserBlockingPriorityKeyword())
    return WebSchedulingPriority::kUserBlockingPriority;
  if (priority == UserVisiblePriorityKeyword())
    return WebSchedulingPriority::kUserVisiblePriority;
  if (priority == BackgroundPriorityKeyword())
    return WebSchedulingPriority::kBackgroundPriority;
  NOTREACHED();
  return WebSchedulingPriority::kUserVisiblePriority;
}

}  // namespace blink
