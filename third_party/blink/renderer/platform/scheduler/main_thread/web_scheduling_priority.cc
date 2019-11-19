// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

namespace {

const AtomicString& ImmediatePriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, immediate_priority, ("immediate"));
  return immediate_priority;
}

const AtomicString& HighPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, high_priority, ("high"));
  return high_priority;
}

const AtomicString& DefaultPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, default_priority, ("default"));
  return default_priority;
}

const AtomicString& LowPriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, low_priority, ("low"));
  return low_priority;
}

const AtomicString& IdlePriorityKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, idle_priority, ("idle"));
  return idle_priority;
}

}  // namespace

AtomicString WebSchedulingPriorityToString(WebSchedulingPriority priority) {
  switch (priority) {
    case WebSchedulingPriority::kImmediatePriority:
      return ImmediatePriorityKeyword();
    case WebSchedulingPriority::kHighPriority:
      return HighPriorityKeyword();
    case WebSchedulingPriority::kDefaultPriority:
      return DefaultPriorityKeyword();
    case WebSchedulingPriority::kLowPriority:
      return LowPriorityKeyword();
    case WebSchedulingPriority::kIdlePriority:
      return IdlePriorityKeyword();
  }

  NOTREACHED();
  return g_empty_atom;
}

WebSchedulingPriority WebSchedulingPriorityFromString(
    const AtomicString& priority) {
  if (priority == ImmediatePriorityKeyword())
    return WebSchedulingPriority::kImmediatePriority;
  if (priority == HighPriorityKeyword())
    return WebSchedulingPriority::kHighPriority;
  if (priority == DefaultPriorityKeyword())
    return WebSchedulingPriority::kDefaultPriority;
  if (priority == LowPriorityKeyword())
    return WebSchedulingPriority::kLowPriority;
  if (priority == IdlePriorityKeyword())
    return WebSchedulingPriority::kIdlePriority;

  NOTREACHED();
  return WebSchedulingPriority::kDefaultPriority;
}

}  // namespace blink
