// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

namespace event_util {

bool IsPointerEventType(const AtomicString& event_type);

bool IsDOMMutationEventType(const AtomicString& event_type);

}  // namespace event_util

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_
