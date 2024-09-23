// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"

#include "base/numerics/checked_math.h"
#include "base/numerics/ostream_operators.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

// Construction of WrapperTypeInfo may require non-trivial initialization due
// to cross-component address resolution in order to load the pointer to the
// parent interface's WrapperTypeInfo.  We ignore this issue because the issue
// happens only on component builds and the official release builds
// (statically-linked builds) are never affected by this issue.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

const WrapperTypeInfo DOMDataView::wrapper_type_info_body_{
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "DataView",
    nullptr,
    kDOMWrappersTag,
    kDOMWrappersTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlBufferSourceType,
};

const WrapperTypeInfo& DOMDataView::wrapper_type_info_ =
    DOMDataView::wrapper_type_info_body_;

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

DOMDataView* DOMDataView::Create(DOMArrayBufferBase* buffer,
                                 size_t byte_offset,
                                 size_t byte_length) {
  base::CheckedNumeric<size_t> checked_max = byte_offset;
  checked_max += byte_length;
  CHECK_LE(checked_max.ValueOrDie(), buffer->ByteLength());
  return MakeGarbageCollected<DOMDataView>(buffer, byte_offset, byte_length);
}

v8::Local<v8::Value> DOMDataView::Wrap(ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(script_state->GetIsolate(), this));

  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();
  v8::Local<v8::Value> v8_buffer =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, buffer());
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
