// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
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

v8::MaybeLocal<v8::Value> DOMDataView::Wrap(ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();
  v8::Local<v8::Value> v8_buffer;
  if (!ToV8Traits<DOMArrayBuffer>::ToV8(script_state, buffer())
           .ToLocal(&v8_buffer)) {
    return v8::MaybeLocal<v8::Value>();
  }
  DCHECK(v8_buffer->IsArrayBuffer());

  v8::Local<v8::Object> wrapper;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    wrapper = v8::DataView::New(v8_buffer.As<v8::ArrayBuffer>(), byteOffset(),
                                byteLength());
  }

  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

}  // namespace blink
