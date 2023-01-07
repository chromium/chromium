// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_error.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

v8::Local<v8::Value> ScriptWebBundleError::ToV8(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  switch (type_) {
    case ScriptWebBundleError::Type::kTypeError:
      return V8ThrowException::CreateTypeError(isolate, message_);
    case ScriptWebBundleError::Type::kSyntaxError:
      return V8ThrowException::CreateSyntaxError(isolate, message_);
    case ScriptWebBundleError::Type::kSystemError:
      return V8ThrowException::CreateError(isolate, message_);
  }
}

}  // namespace blink
