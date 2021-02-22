// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"

namespace blink {

v8::Local<v8::Value> DOMSharedArrayBuffer::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::SharedArrayBuffer> wrapper =
      v8::SharedArrayBuffer::New(isolate, Content()->BackingStore());
  wrapper->Externalize(Content()->BackingStore());

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
}

v8::MaybeLocal<v8::Value> DOMSharedArrayBuffer::WrapV2(
    ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();
  v8::Local<v8::SharedArrayBuffer> wrapper = v8::SharedArrayBuffer::New(
      script_state->GetIsolate(), Content()->BackingStore());
  wrapper->Externalize(Content()->BackingStore());

  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

}  // namespace blink
