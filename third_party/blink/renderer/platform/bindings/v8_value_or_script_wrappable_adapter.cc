// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_value_or_script_wrappable_adapter.h"

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
namespace bindings {

v8::Local<v8::Value> V8ValueOrScriptWrappableAdapter::V8Value(
    ScriptState* script_state) const {
  // Only one of two must be set.
  DCHECK(!v8_value_.IsEmpty() || script_wrappable_);
  DCHECK(!(!v8_value_.IsEmpty() && script_wrappable_));

  if (!v8_value_.IsEmpty())
    return v8_value_;

  return script_wrappable_->ToV8(script_state);
}

}  // namespace bindings
}  // namespace blink
