// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_DICTIONARY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_DICTIONARY_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

// This class provides toV8Impl() virtual function which will be overridden
// by auto-generated IDL dictionary impl classes. toV8Impl() is used
// in ToV8.h to provide a consistent API of ToV8().
class CORE_EXPORT IDLDictionaryBase
    : public GarbageCollected<IDLDictionaryBase> {
 public:
  virtual ~IDLDictionaryBase() = default;

  virtual v8::Local<v8::Value> ToV8Impl(v8::Local<v8::Object> creation_context,
                                        v8::Isolate*) const;

  virtual void Trace(Visitor*) const;

 protected:
  IDLDictionaryBase() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_DICTIONARY_BASE_H_
