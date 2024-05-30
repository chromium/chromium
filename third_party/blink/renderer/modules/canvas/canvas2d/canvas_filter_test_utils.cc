// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_test_utils.h"

#include <string>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray_string.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

namespace blink_testing {

blink::V8UnionObjectOrObjectArrayOrString* ParseFilter(
    blink::V8TestingScope& scope,
    const std::string& value) {
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(scope.GetIsolate(), value.c_str())
          .ToLocalChecked();
  v8::Local<v8::Script> script =
      v8::Script::Compile(scope.GetContext(), source).ToLocalChecked();
  return blink::V8UnionObjectOrObjectArrayOrString::Create(
      scope.GetIsolate(), script->Run(scope.GetContext()).ToLocalChecked(),
      scope.GetExceptionState());
}

}  // namespace blink_testing
