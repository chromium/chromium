// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_TEST_UTIL_H_

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

namespace blink {
class Event;
class EventTarget;
class Node;
class PerformanceEventTiming;
class ScriptState;
class TextRecord;

PerformanceEventTiming* CreatePerformanceEventTimingForTest(
    const AtomicString& event_type,
    base::TimeTicks start_time,
    EventTarget* target,
    DOMWindow* source);

// Creates a `TextRecord` associated with `context` and `node` with the given
// dimensions. The `width` and `height` are used for all relevant gfx::Rect
// objects, as well as the recorded size.
TextRecord* CreateTextRecordForTest(Node* node,
                                    int width,
                                    int height,
                                    SoftNavigationContext* context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_HEURISTICS_TEST_UTIL_H_
