// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_push_event_init.h"

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
    const ArrayBufferOrArrayBufferViewOrUSVString& message_data =
        initializer->data();
    if (message_data.IsArrayBuffer() || message_data.IsArrayBufferView()) {
      DOMArrayBuffer* buffer =
          message_data.IsArrayBufferView()
              ? message_data.GetAsArrayBufferView().View()->buffer()
              : message_data.GetAsArrayBuffer();
      if (!base::CheckedNumeric<uint32_t>(buffer->ByteLength()).IsValid()) {
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
