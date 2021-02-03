// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"

namespace blink {

namespace {

TEST(ToV8TraitsTest, JustUsingToV8) {
  V8TestingScope scope;
  DOMPoint* point = DOMPoint::Create(1, 2, 3, 4);
  v8::Local<v8::Value> v8_value;
  if (!ToV8Traits<DOMPoint>::ToV8(scope.GetScriptState(), point)
           .ToLocal(&v8_value)) {
    return;
  }
}

}  // namespace

}  // namespace blink
