// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_message_data.h"

#include <memory>

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_usvstring.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "v8/include/v8.h"

namespace blink {

PushMessageData* PushMessageData::Create(const String& message_string) {
  // The standard supports both an empty but valid message and a null message.
  // In case the message is explicitly null, return a null pointer which will
  // be set in the PushEvent.
  if (message_string.IsNull())
    return nullptr;
  return PushMessageData::Create(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrUSVString>(
          message_string));
}

PushMessageData* PushMessageData::Create(
    const V8UnionArrayBufferOrArrayBufferViewOrUSVString* message_data) {
  if (!message_data)
    return nullptr;
  switch (message_data->GetContentType()) {
    case V8UnionArrayBufferOrArrayBufferViewOrUSVString::ContentType::
        kArrayBuffer: {
      const DOMArrayBuffer* buffer = message_data->GetAsArrayBuffer();
      return MakeGarbageCollected<PushMessageData>(buffer->ByteSpan());
    }
    case V8UnionArrayBufferOrArrayBufferViewOrUSVString::ContentType::
        kArrayBufferView: {
      const DOMArrayBufferView* buffer_view =
          message_data->GetAsArrayBufferView().Get();
      return MakeGarbageCollected<PushMessageData>(buffer_view->ByteSpan());
    }
    case V8UnionArrayBufferOrArrayBufferViewOrUSVString::ContentType::
        kUSVString: {
      std::string encoded_string = UTF8Encoding().Encode(
          message_data->GetAsUSVString(), WTF::kNoUnencodables);
      return MakeGarbageCollected<PushMessageData>(
          base::as_byte_span(encoded_string));
    }
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

PushMessageData::PushMessageData(base::span<const uint8_t> data) {
  data_.AppendSpan(data);
}

PushMessageData::~PushMessageData() = default;

DOMArrayBuffer* PushMessageData::arrayBuffer() const {
  return DOMArrayBuffer::Create(data_);
}

Blob* PushMessageData::blob() const {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendBytes(data_);

  // Note that the content type of the Blob object is deliberately not being
  // provided, following the specification.

  const uint64_t byte_length = blob_data->length();
  return MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), byte_length));
}

ScriptValue PushMessageData::json(ScriptState* script_state,
                                  ExceptionState& exception_state) const {
  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> parsed =
      FromJSONString(script_state->GetIsolate(), script_state->GetContext(),
                     text(), exception_state);
  if (exception_state.HadException())
    return ScriptValue();

  return ScriptValue(script_state->GetIsolate(), parsed);
}

String PushMessageData::text() const {
  return UTF8Encoding().Decode(data_);
}

}  // namespace blink
