/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_WRAPPABLE_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "v8/include/v8.h"

namespace blink {

// ScriptWrappable provides a way to map from/to C++ DOM implementation to/from
// JavaScript object (platform object).  ToV8() converts a ScriptWrappable to
// a v8::Object and toScriptWrappable() converts a v8::Object back to
// a ScriptWrappable.  v8::Object as platform object is called "wrapper object".
// The wrapper object for the main world is stored in ScriptWrappable.  Wrapper
// objects for other worlds are stored in DOMWrapperMap.
class PLATFORM_EXPORT ScriptWrappable
    : public GarbageCollected<ScriptWrappable>,
      public NameClient {
 public:
  virtual ~ScriptWrappable() = default;

  // The following methods may override lifetime of ScriptWrappable objects when
  // needed. In particular if |HasPendingActivity| or |HasEventListeners|
  // returns true *and* the child type also inherits from
  // |ActiveScriptWrappable|, the objects will not be reclaimed by the GC, even
  // if they are otherwise unreachable.
  //
  // Note: These methods are queried during garbage collection and *must not*
  // allocate any new objects.
  virtual bool HasPendingActivity() const { return false; }
  virtual bool HasEventListeners() const { return false; }

  const char* NameInHeapSnapshot() const override;

  virtual void Trace(blink::Visitor*);

  template <typename T>
  T* ToImpl() {
    // All ScriptWrappables are managed by the Blink GC heap; check that
    // |T| is a garbage collected type.
    static_assert(
        sizeof(T) && WTF::IsGarbageCollectedType<T>::value,
        "Classes implementing ScriptWrappable must be garbage collected.");

// Check if T* is castable to ScriptWrappable*, which means T doesn't
// have two or more ScriptWrappable as superclasses. If T has two
// ScriptWrappable as superclasses, conversions from T* to
// ScriptWrappable* are ambiguous.
#if !defined(COMPILER_MSVC)
    // MSVC 2013 doesn't support static_assert + constexpr well.
    static_assert(!static_cast<ScriptWrappable*>(static_cast<T*>(nullptr)),
                  "Class T must not have two or more ScriptWrappable as its "
                  "superclasses.");
#endif
    return static_cast<T*>(this);
  }

  // Returns the WrapperTypeInfo of the instance.
  //
  // This method must be overridden by DEFINE_WRAPPERTYPEINFO macro.
  virtual const WrapperTypeInfo* GetWrapperTypeInfo() const = 0;

  // Creates and returns a new wrapper object.
  virtual v8::Local<v8::Object> Wrap(v8::Isolate*,
                                     v8::Local<v8::Object> creation_context);

  // Associates the instance with the given |wrapper| if this instance is not
  // yet associated with any wrapper.  Returns the wrapper already associated
  // or |wrapper| if not yet associated.
  // The caller should always use the returned value rather than |wrapper|.
  WARN_UNUSED_RESULT virtual v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper);

  // Associates this instance with the given |wrapper| if this instance is not
  // yet associated with any wrapper.  Returns true if the given wrapper is
  // associated with this instance, or false if this instance is already
  // associated with a wrapper.  In the latter case, |wrapper| will be updated
  // to the existing wrapper.
  WARN_UNUSED_RESULT bool SetWrapper(v8::Isolate* isolate,
                                     const WrapperTypeInfo* wrapper_type_info,
                                     v8::Local<v8::Object>& wrapper) {
    DCHECK(!wrapper.IsEmpty());
    if (UNLIKELY(ContainsWrapper())) {
      wrapper = MainWorldWrapper(isolate);
      return false;
    }
    main_world_wrapper_.Set(isolate, wrapper);
    DCHECK(ContainsWrapper());
    wrapper_type_info->ConfigureWrapper(&main_world_wrapper_.Get());
    return true;
  }

  bool IsEqualTo(const v8::Local<v8::Object>& other) const {
    return main_world_wrapper_.Get() == other;
  }

  bool SetReturnValue(v8::ReturnValue<v8::Value> return_value) {
    return_value.Set(main_world_wrapper_.Get());
    return ContainsWrapper();
  }

  bool ContainsWrapper() const { return !main_world_wrapper_.IsEmpty(); }

 protected:
  ScriptWrappable() = default;

 private:
  v8::Local<v8::Object> MainWorldWrapper(v8::Isolate* isolate) const {
    return main_world_wrapper_.NewLocal(isolate);
  }

  // Clear the main world wrapper if it is set to |handle|.
  bool UnsetMainWorldWrapperIfSet(
      const v8::TracedReference<v8::Object>& handle);

  static_assert(
      std::is_trivially_destructible<
          TraceWrapperV8Reference<v8::Object>>::value,
      "TraceWrapperV8Reference<v8::Object> should be trivially destructible.");

  TraceWrapperV8Reference<v8::Object> main_world_wrapper_;

  // These classes are exceptionally allowed to directly interact with the main
  // world wrapper.
  friend class DOMDataStore;
  friend class DOMWrapperWorld;
  friend class HeapSnaphotWrapperVisitor;
  friend class V8HiddenValue;
  friend class V8PrivateProperty;

  DISALLOW_COPY_AND_ASSIGN(ScriptWrappable);
};

inline bool ScriptWrappable::UnsetMainWorldWrapperIfSet(
    const v8::TracedReference<v8::Object>& handle) {
  if (main_world_wrapper_.Get() == handle) {
    main_world_wrapper_.Clear();
    return true;
  }
  return false;
}

// Defines |GetWrapperTypeInfo| virtual method which returns the WrapperTypeInfo
// of the instance. Also declares a static member of type WrapperTypeInfo, of
// which the definition is given by the IDL code generator.
//
// All the derived classes of ScriptWrappable, regardless of directly or
// indirectly, must write this macro in the class definition as long as the
// class has a corresponding .idl file.
#define DEFINE_WRAPPERTYPEINFO()                               \
 public:                                                       \
  const WrapperTypeInfo* GetWrapperTypeInfo() const override { \
    return &wrapper_type_info_;                                \
  }                                                            \
                                                               \
 private:                                                      \
  static const WrapperTypeInfo& wrapper_type_info_

// Declares |GetWrapperTypeInfo| method without definition.
//
// This macro is used for template classes. e.g. DOMTypedArray<>.
// To export such a template class X, we need to instantiate X with EXPORT_API,
// i.e. "extern template class EXPORT_API X;"
// However, once we instantiate X, we cannot specialize X after
// the instantiation. i.e. we will see "error: explicit specialization of ...
// after instantiation". So we cannot define X's s_wrapper_type_info in
// generated code by using specialization. Instead, we need to implement
// wrapper_type_info in X's cpp code, and instantiate X, i.e. "template class
// X;".
#define DECLARE_WRAPPERTYPEINFO()                             \
 public:                                                      \
  const WrapperTypeInfo* GetWrapperTypeInfo() const override; \
                                                              \
 private:                                                     \
  typedef void end_of_declare_wrappertypeinfo_t

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_WRAPPABLE_H_
