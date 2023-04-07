// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader_presence_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

SmartCardReaderPresenceObserver::SmartCardReaderPresenceObserver(
    ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      blink::ActiveScriptWrappable<SmartCardReaderPresenceObserver>({}) {}

SmartCardReaderPresenceObserver::~SmartCardReaderPresenceObserver() = default;

ExecutionContext* SmartCardReaderPresenceObserver::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& SmartCardReaderPresenceObserver::InterfaceName() const {
  return event_target_names::kSmartCardReaderPresenceObserver;
}

bool SmartCardReaderPresenceObserver::HasPendingActivity() const {
  return HasEventListeners();
}

void SmartCardReaderPresenceObserver::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void SmartCardReaderPresenceObserver::ContextDestroyed() {}

}  // namespace blink
