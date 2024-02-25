// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/sync_iterator_base.h"

namespace blink::bindings {

v8::Local<v8::Object> SyncIteratorBase::next(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return iteration_source_->Next(script_state, kind_, exception_state);
}

void SyncIteratorBase::Trace(Visitor* visitor) const {
  visitor->Trace(iteration_source_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink::bindings
