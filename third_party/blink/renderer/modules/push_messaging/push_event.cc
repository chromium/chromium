// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_event_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

PushEvent::PushEvent(const AtomicString& type,
                     PushMessageData* data,
                     WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit::Create(), observer),
      data_(data) {}

PushEvent::PushEvent(const AtomicString& type,
                     const PushEventInit* initializer,
                     ExceptionState& exception_state)
    : ExtendableEvent(type, initializer) {
  if (initializer->hasData()) {
    const auto* message_data = initializer->data();
    if (message_data->IsArrayBuffer() || message_data->IsArrayBufferView()) {
      base::span<const uint8_t> span =
          AsSpan<SharedBufferPolicy::kDisallow>(*message_data);
      if (!base::CheckedNumeric<uint32_t>(span.size()).IsValid()) {
        exception_state.ThrowRangeError(
            "The provided ArrayBuffer exceeds the maximum supported size "
            "(4294967295)");
        return;
      }
    }
    data_ = PushMessageData::Create(initializer->data());
  }
}

PushEvent::~PushEvent() = default;

const AtomicString& PushEvent::InterfaceName() const {
  return event_interface_names::kPushEvent;
}

PushMessageData* PushEvent::data() {
  return data_.Get();
}

void PushEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  ExtendableEvent::Trace(visitor);
}

}  // namespace blink
