// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader.h"

#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_resource_manager.h"

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

device::mojom::blink::SmartCardProtocolsPtr ToMojoProtocols(
    const Vector<V8SmartCardProtocol>& preferred_protocols) {
  auto result = device::mojom::blink::SmartCardProtocols::New();

  for (const auto& protocol : preferred_protocols) {
    switch (protocol.AsEnum()) {
      case blink::V8SmartCardProtocol::Enum::kRaw:
        result->raw = true;
        break;
      case blink::V8SmartCardProtocol::Enum::kT0:
        result->t0 = true;
        break;
      case blink::V8SmartCardProtocol::Enum::kT1:
        result->t1 = true;
        break;
    }
  }

  return result;
}

device::mojom::blink::SmartCardShareMode ToMojoShareMode(
    V8SmartCardAccessMode access_mode) {
  switch (access_mode.AsEnum()) {
    case blink::V8SmartCardAccessMode::Enum::kShared:
      return device::mojom::blink::SmartCardShareMode::kShared;
    case blink::V8SmartCardAccessMode::Enum::kExclusive:
      return device::mojom::blink::SmartCardShareMode::kExclusive;
    case blink::V8SmartCardAccessMode::Enum::kDirect:
      return device::mojom::blink::SmartCardShareMode::kDirect;
  }
}
}  // anonymous namespace

SmartCardReader::SmartCardReader(SmartCardResourceManager* resource_manager,
                                 SmartCardReaderInfoPtr info,
                                 ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      ActiveScriptWrappable<SmartCardReader>({}),
      resource_manager_(resource_manager),
      name_(info->name),
      state_(V8StateFromMojoState(info->state)),
      atr_(info->atr) {}

SmartCardReader::~SmartCardReader() = default;

ScriptPromise SmartCardReader::connect(ScriptState* script_state,
                                       V8SmartCardAccessMode access_mode,
                                       ExceptionState& exception_state) {
  return connect(script_state, access_mode, Vector<V8SmartCardProtocol>(),
                 exception_state);
}

ScriptPromise SmartCardReader::connect(
    ScriptState* script_state,
    V8SmartCardAccessMode access_mode,
    const Vector<V8SmartCardProtocol>& preferred_protocols,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  connect_promises_.insert(resolver);

  resource_manager_->Connect(
      name_, ToMojoShareMode(access_mode), ToMojoProtocols(preferred_protocols),
      WTF::BindOnce(&SmartCardReader::OnConnectDone, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return resolver->Promise();
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
  visitor->Trace(resource_manager_);
  visitor->Trace(connect_promises_);
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

void SmartCardReader::OnConnectDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardConnectResultPtr result) {
  CHECK(connect_promises_.Contains(resolver));
  connect_promises_.erase(resolver);

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  auto* connection = MakeGarbageCollected<SmartCardConnection>(
      std::move(result->get_success()->connection), GetExecutionContext());

  resolver->Resolve(connection);
}

}  // namespace blink
