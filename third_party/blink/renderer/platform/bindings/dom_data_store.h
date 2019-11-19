/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_DATA_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_DATA_STORE_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_marking_visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {

// Holds a map specialized to map between ScriptWrappable objects and their
// wrappers and provides an API to perform common operations with this map and
// manage wrappers in a single world. Each world (DOMWrapperWorld) holds a
// single map instance to hold wrappers only for that world.
class DOMDataStore final : public GarbageCollected<DOMDataStore> {
 public:
  static DOMDataStore& Current(v8::Isolate* isolate) {
    return DOMWrapperWorld::Current(isolate).DomDataStore();
  }

  static bool SetReturnValue(v8::ReturnValue<v8::Value> return_value,
                             ScriptWrappable* object) {
    if (CanUseMainWorldWrapper())
      return object->SetReturnValue(return_value);
    return Current(return_value.GetIsolate())
        .SetReturnValueFrom(return_value, object);
  }

  static bool SetReturnValueForMainWorld(
      v8::ReturnValue<v8::Value> return_value,
      ScriptWrappable* object) {
    return object->SetReturnValue(return_value);
  }

  static bool SetReturnValueFast(v8::ReturnValue<v8::Value> return_value,
                                 ScriptWrappable* object,
                                 v8::Local<v8::Object> holder,
                                 const ScriptWrappable* wrappable) {
    if (CanUseMainWorldWrapper()
        // The second fastest way to check if we're in the main world is to
        // check if the wrappable's wrapper is the same as the holder.
        || HolderContainsWrapper(holder, wrappable))
      return object->SetReturnValue(return_value);
    return Current(return_value.GetIsolate())
        .SetReturnValueFrom(return_value, object);
  }

  static v8::Local<v8::Object> GetWrapper(ScriptWrappable* object,
                                          v8::Isolate* isolate) {
    if (CanUseMainWorldWrapper())
      return object->MainWorldWrapper(isolate);
    return Current(isolate).Get(object, isolate);
  }

  // Associates the given |object| with the given |wrapper| if the object is
  // not yet associated with any wrapper.  Returns true if the given wrapper
  // is associated with the object, or false if the object is already
  // associated with a wrapper.  In the latter case, |wrapper| will be updated
  // to the existing wrapper.
  WARN_UNUSED_RESULT static bool SetWrapper(
      v8::Isolate* isolate,
      ScriptWrappable* object,
      const WrapperTypeInfo* wrapper_type_info,
      v8::Local<v8::Object>& wrapper) {
    if (CanUseMainWorldWrapper())
      return object->SetWrapper(isolate, wrapper_type_info, wrapper);
    return Current(isolate).Set(isolate, object, wrapper_type_info, wrapper);
  }

  static bool ContainsWrapper(const ScriptWrappable* object,
                              v8::Isolate* isolate) {
    return Current(isolate).ContainsWrapper(object);
  }

  DOMDataStore(v8::Isolate* isolate, bool is_main_world);

  // Clears all references.
  void Dispose();

  v8::Local<v8::Object> Get(ScriptWrappable* object, v8::Isolate* isolate) {
    if (is_main_world_)
      return object->MainWorldWrapper(isolate);
    auto it = wrapper_map_.find(object);
    if (it != wrapper_map_.end())
      return it->value->ref.NewLocal(isolate);
    return v8::Local<v8::Object>();
  }

  WARN_UNUSED_RESULT bool Set(v8::Isolate* isolate,
                              ScriptWrappable* object,
                              const WrapperTypeInfo* wrapper_type_info,
                              v8::Local<v8::Object>& wrapper) {
    DCHECK(object);
    DCHECK(!wrapper.IsEmpty());
    if (is_main_world_)
      return object->SetWrapper(isolate, wrapper_type_info, wrapper);

    auto result = wrapper_map_.insert(
        object, MakeGarbageCollected<WrappedReference>(isolate, wrapper));
    if (LIKELY(result.is_new_entry)) {
      wrapper_type_info->ConfigureWrapper(
          &result.stored_value->value->ref.Get());
    } else {
      DCHECK(!result.stored_value->value->ref.IsEmpty());
      wrapper = result.stored_value->value->ref.NewLocal(isolate);
    }
    return result.is_new_entry;
  }

  bool UnsetSpecificWrapperIfSet(
      ScriptWrappable* object,
      const v8::TracedReference<v8::Object>& handle) {
    DCHECK(!is_main_world_);
    const auto& it = wrapper_map_.find(object);
    if (it != wrapper_map_.end()) {
      if (it->value->ref.Get() == handle) {
        it->value->ref.Clear();
        wrapper_map_.erase(it);
        return true;
      }
    }
    return false;
  }

  bool SetReturnValueFrom(v8::ReturnValue<v8::Value> return_value,
                          ScriptWrappable* object) {
    if (is_main_world_)
      return object->SetReturnValue(return_value);
    auto it = wrapper_map_.find(object);
    if (it != wrapper_map_.end()) {
      return_value.Set(it->value->ref.Get());
      return true;
    }
    return false;
  }

  bool ContainsWrapper(const ScriptWrappable* object) {
    if (is_main_world_)
      return object->ContainsWrapper();
    return wrapper_map_.find(object) != wrapper_map_.end();
  }

  virtual void Trace(Visitor*);

 private:
  // We can use a wrapper stored in a ScriptWrappable when we're in the main
  // world.  This method does the fast check if we're in the main world. If this
  // method returns true, it is guaranteed that we're in the main world. On the
  // other hand, if this method returns false, nothing is guaranteed (we might
  // be in the main world).
  static bool CanUseMainWorldWrapper() {
    return !WTF::MayNotBeMainThread() &&
           !DOMWrapperWorld::NonMainWorldsExistInMainThread();
  }

  static bool HolderContainsWrapper(v8::Local<v8::Object> holder,
                                    const ScriptWrappable* wrappable) {
    // Verify our assumptions about the main world.
    DCHECK(wrappable);
    DCHECK(!wrappable->ContainsWrapper() || !wrappable->IsEqualTo(holder) ||
           Current(v8::Isolate::GetCurrent()).is_main_world_);
    return wrappable->IsEqualTo(holder);
  }

  // Wrapper around TraceWrapperV8Reference to allow use in ephemeron hash map
  // below.
  class PLATFORM_EXPORT WrappedReference final
      : public GarbageCollected<WrappedReference> {
   public:
    WrappedReference() = default;
    WrappedReference(v8::Isolate* isolate, v8::Local<v8::Object> handle)
        : ref(isolate, handle) {}

    virtual void Trace(Visitor*);

    TraceWrapperV8Reference<v8::Object> ref;
  };

  bool is_main_world_;
  // Ephemeron map: WrappedReference will be kept alive as long as
  // ScriptWrappable is alive.
  HeapHashMap<WeakMember<const ScriptWrappable>, Member<WrappedReference>>
      wrapper_map_;

  DISALLOW_COPY_AND_ASSIGN(DOMDataStore);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DOM_DATA_STORE_H_
