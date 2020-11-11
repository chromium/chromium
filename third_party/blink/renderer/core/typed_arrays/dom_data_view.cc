// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

DOMDataView* DOMDataView::Create(DOMArrayBufferBase* buffer,
                                 size_t byte_offset,
                                 size_t byte_length) {
  base::CheckedNumeric<size_t> checked_max = byte_offset;
  checked_max += byte_length;
  CHECK_LE(checked_max.ValueOrDie(), buffer->ByteLength());
  return MakeGarbageCollected<DOMDataView>(buffer, byte_offset, byte_length);
}

v8::Local<v8::Value> DOMDataView::Wrap(v8::Isolate* isolate,
                                       v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::Value> v8_buffer = ToV8(buffer(), creation_context, isolate);
  if (v8_buffer.IsEmpty())
    return v8::Local<v8::Object>();
  DCHECK(v8_buffer->IsArrayBuffer());

  v8::Local<v8::Object> wrapper = v8::DataView::New(
      v8_buffer.As<v8::ArrayBuffer>(), byteOffset(), byteLength());

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

}  // namespace blink
