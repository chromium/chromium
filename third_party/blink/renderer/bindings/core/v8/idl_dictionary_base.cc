// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"

namespace blink {

v8::Local<v8::Value> IDLDictionaryBase::ToV8Impl(v8::Local<v8::Object>,
                                                 v8::Isolate*) const {
  NOTREACHED();
  return v8::Local<v8::Value>();
}

void IDLDictionaryBase::Trace(Visitor* visitor) const {}

}  // namespace blink
