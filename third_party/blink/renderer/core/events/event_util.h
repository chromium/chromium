// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

namespace event_util {

CORE_EXPORT const Vector<AtomicString>& MouseButtonEventTypes();

CORE_EXPORT bool IsMouseButtonEventType(const AtomicString& event_type);

CORE_EXPORT bool IsPointerEventType(const AtomicString& event_type);

// |web_feature| and |listener_type| are outputs, corresponding to the
// provided |event_type|. They will not be changed if the provided |event_type|
// is not a DOM Mutation Event.
bool IsDOMMutationEventType(const AtomicString& event_type,
                            WebFeature& web_feature,
                            Document::ListenerType& listener_type);
bool IsDOMMutationEventType(const AtomicString& event_type);

}  // namespace event_util

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_EVENT_UTIL_H_
