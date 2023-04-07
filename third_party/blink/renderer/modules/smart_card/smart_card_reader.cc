// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {
namespace {
V8SmartCardReaderState::Enum V8StateFromMojoState(
    mojom::blink::SmartCardReaderState state) {
  switch (state) {
    case mojom::blink::SmartCardReaderState::kUnavailable:
      return V8SmartCardReaderState::Enum::kUnavailable;
    case mojom::blink::SmartCardReaderState::kEmpty:
      return V8SmartCardReaderState::Enum::kEmpty;
    case mojom::blink::SmartCardReaderState::kPresent:
      return V8SmartCardReaderState::Enum::kPresent;
    case mojom::blink::SmartCardReaderState::kExclusive:
      return V8SmartCardReaderState::Enum::kExclusive;
    case mojom::blink::SmartCardReaderState::kInuse:
      return V8SmartCardReaderState::Enum::kInuse;
    case mojom::blink::SmartCardReaderState::kMute:
      return V8SmartCardReaderState::Enum::kMute;
    case mojom::blink::SmartCardReaderState::kUnpowered:
      return V8SmartCardReaderState::Enum::kUnpowered;
  }
}
}  // anonymous namespace

SmartCardReader::SmartCardReader(SmartCardReaderInfoPtr info,
                                 ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      ActiveScriptWrappable<SmartCardReader>({}),
      name_(info->name),
      state_(V8StateFromMojoState(info->state)),
      atr_(info->atr) {}

SmartCardReader::~SmartCardReader() = default;

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

void SmartCardReader::UpdateInfo(SmartCardReaderInfoPtr info) {
  // name is constant
  CHECK_EQ(info->name, name_);

  V8SmartCardReaderState::Enum new_state_enum =
      V8StateFromMojoState(info->state);
  if (state_ != new_state_enum) {
    state_ = V8SmartCardReaderState(new_state_enum);
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }

  // TODO(crbug.com/1386175): Dispatch kAtrchange event when appropriate.
  atr_ = info->atr;
}

}  // namespace blink
