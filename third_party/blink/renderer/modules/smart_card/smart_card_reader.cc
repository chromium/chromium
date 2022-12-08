// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

SmartCardReader::SmartCardReader(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context) {}

SmartCardReader::~SmartCardReader() = default;

const String& SmartCardReader::name() const {
  NOTIMPLEMENTED();
  return g_empty_string;
}

ExecutionContext* SmartCardReader::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& SmartCardReader::InterfaceName() const {
  return event_target_names::kSmartCardReader;
}

bool SmartCardReader::HasPendingActivity() const {
  return HasEventListeners();
}

void SmartCardReader::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void SmartCardReader::ContextDestroyed() {}

}  // namespace blink
