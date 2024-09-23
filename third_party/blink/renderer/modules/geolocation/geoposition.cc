// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/geolocation/geoposition.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

ScriptValue Geoposition::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddInteger("timestamp", timestamp_);
  builder.AddV8Value("coords", coordinates_->toJSON(script_state).V8Value());
  return builder.GetScriptValue();
}

}  // namespace blink
