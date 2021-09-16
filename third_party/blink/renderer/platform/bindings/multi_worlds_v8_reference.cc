// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/multi_worlds_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

MultiWorldsV8Reference::MultiWorldsV8Reference(v8::Isolate* isolate,
                                               v8::Local<v8::Object> object)
    : object_(isolate, object),
      script_state_(ScriptState::From(object->CreationContext())) {}

v8::Local<v8::Object> MultiWorldsV8Reference::GetObject(
    ScriptState* script_state) {
  if (&script_state->World() == &script_state_->World()) {
    return object_.NewLocal(script_state_->GetIsolate());
  } else {
    // TODO(nonoohara): We will create an object that is a clone of object_
    // and put it in copy_object.
    NOTIMPLEMENTED();
    v8::Local<v8::Object>
        copy_object;  // Suppose it contains a copy of the object.
    return copy_object;
  }
}

void MultiWorldsV8Reference::Trace(Visitor* visitor) const {
  visitor->Trace(object_);
  visitor->Trace(script_state_);
}

}  // namespace blink
