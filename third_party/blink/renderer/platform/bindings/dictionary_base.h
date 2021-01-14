// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

namespace bindings {

// This class is the base class for all IDL dictionary implementations.  This is
// designed to collaborate with NativeValueTraits and ToV8 with supporting type
// dispatching (SFINAE, etc.).
class PLATFORM_EXPORT DictionaryBase : public GarbageCollected<DictionaryBase> {
 public:
  virtual ~DictionaryBase() = default;

  v8::Local<v8::Value> CreateV8Object(
      v8::Isolate* isolate,
      v8::Local<v8::Object> creation_context) const {
    v8::Local<v8::Context> context = creation_context->CreationContext();
    DCHECK(!context.IsEmpty());
    v8::Local<v8::Object> v8_object;
    {
      v8::Context::Scope context_scope(context);
      v8_object = v8::Object::New(isolate);
    }
    FillWithMembers(isolate, creation_context, v8_object);
    return v8_object;
  }

  virtual void Trace(Visitor*) const {}

 protected:
  DictionaryBase() = default;
  explicit DictionaryBase(v8::Isolate* isolate) {}

  DictionaryBase(const DictionaryBase&) = delete;
  DictionaryBase(const DictionaryBase&&) = delete;
  DictionaryBase& operator=(const DictionaryBase&) = delete;
  DictionaryBase& operator=(const DictionaryBase&&) = delete;

  virtual bool FillWithMembers(v8::Isolate* isolate,
                               v8::Local<v8::Object> creation_context,
                               v8::Local<v8::Object> v8_object) const = 0;
};

}  // namespace bindings
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
