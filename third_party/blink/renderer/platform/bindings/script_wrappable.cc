// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsScriptWrappable {
  virtual ~SameSizeAsScriptWrappable() = default;
  v8::Persistent<v8::Object> main_world_wrapper_;
};

ASSERT_SIZE(ScriptWrappable, SameSizeAsScriptWrappable);

v8::MaybeLocal<v8::Value> ScriptWrappable::Wrap(ScriptState* script_state) {
  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();

  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  v8::Local<v8::Object> wrapper;
  if (!V8DOMWrapper::CreateWrapper(script_state, wrapper_type_info)
           .ToLocal(&wrapper)) {
    return v8::MaybeLocal<v8::Value>();
  }
  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

v8::Local<v8::Object> ScriptWrappable::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  return V8DOMWrapper::AssociateObjectWithWrapper(isolate, this,
                                                  wrapper_type_info, wrapper);
}

void ScriptWrappable::Trace(Visitor* visitor) const {
  visitor->Trace(main_world_wrapper_);
}

const char* ScriptWrappable::NameInHeapSnapshot() const {
  return GetWrapperTypeInfo()->interface_name;
}

}  // namespace blink
