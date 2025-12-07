// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BOXED_V8_MODULE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BOXED_V8_MODULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "v8/include/v8.h"

namespace blink {

// BoxedV8Module wraps a handle to a v8::Module for use on heap.
//
// Hash of Member<BoxedV8Module> overrides HashTraits.
// Therefore, BoxedV8Module can be a key/value type of blink::HashMap and
// blink::HashSet by using BoxedV8ModuleHash.
class CORE_EXPORT BoxedV8Module final : public GarbageCollected<BoxedV8Module> {
 public:
  BoxedV8Module(v8::Isolate* isolate, v8::Local<v8::Module> module)
      : record_(isolate, module),
        identity_hash_(static_cast<unsigned>(module->GetIdentityHash())) {}

  BoxedV8Module(v8::Isolate* isolate, v8::Local<v8::WasmModuleObject> module)
      : record_(isolate, module),
        identity_hash_(static_cast<unsigned>(module->GetIdentityHash())) {}

  void Trace(Visitor* visitor) const { visitor->Trace(record_); }

  v8::Local<v8::Module> NewLocal(v8::Isolate* isolate) const {
    v8::Local<v8::Data> record = record_.Get(isolate);
    CHECK(record->IsModule());
    return record.As<v8::Module>();
  }

  v8::Local<v8::WasmModuleObject> NewWasmLocal(v8::Isolate* isolate) const {
    v8::Local<v8::Data> record = record_.Get(isolate);
    CHECK(record->IsValue());
    v8::Local<v8::Value> record_value = record.As<v8::Value>();
    CHECK(record_value->IsWasmModuleObject());
    return record_value.As<v8::WasmModuleObject>();
  }

 private:
  // Must be either `v8::Module` or `v8::WasmModuleObject`.
  TraceWrapperV8Reference<v8::Data> record_;
  const unsigned identity_hash_;
  friend struct HashTraits<Member<BoxedV8Module>>;
};

template <>
struct HashTraits<Member<BoxedV8Module>> : MemberHashTraits<BoxedV8Module> {
  static unsigned GetHash(const Member<BoxedV8Module>& key) {
    return key->identity_hash_;
  }

  static bool Equal(const Member<BoxedV8Module>& a,
                    const Member<BoxedV8Module>& b) {
    return a->record_ == b->record_;
  }

  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BOXED_V8_MODULE_H_
