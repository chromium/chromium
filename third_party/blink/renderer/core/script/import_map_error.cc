// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/import_map_error.h"
#include "third_party/blink/renderer/core/script/pending_import_map.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

v8::Local<v8::Value> ImportMapError::ToV8(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  switch (type_) {
    case ImportMapError::Type::kTypeError:
      return V8ThrowException::CreateTypeError(isolate, message_);
    case ImportMapError::Type::kSyntaxError:
      return V8ThrowException::CreateSyntaxError(isolate, message_);
  }
}

}  // namespace blink
