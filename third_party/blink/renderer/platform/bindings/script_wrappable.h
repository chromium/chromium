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

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptState;

// ScriptWrappable provides a way to map from/to C++ DOM implementation to/from
// JavaScript object (platform object).  ToV8() converts a ScriptWrappable to
// a v8::Object and toScriptWrappable() converts a v8::Object back to
// a ScriptWrappable.  v8::Object as platform object is called "wrapper object".
// The wrapper object for the main world is stored in ScriptWrappable.  Wrapper
// objects for other worlds are stored in DOMDataStore.
class PLATFORM_EXPORT ScriptWrappable
    : public GarbageCollected<ScriptWrappable>,
      public NameClient {
 public:
  // This is a type dispatcher from ScriptWrappable* to a subtype, optimized for
  // use cases that perform downcasts multiple times. If you perform a downcast
  // only once, ScriptWrappable::DowncastTo or ScriptWrappable::ToMostDerived
  // would be a better choice.
  class TypeDispatcher final {
    STACK_ALLOCATED();

   public:
    // The input parameter `script_wrappable` must not be null.
    explicit TypeDispatcher(ScriptWrappable* script_wrappable)
        : script_wrappable_(script_wrappable),
          wrapper_type_info_(script_wrappable->GetWrapperTypeInfo()) {}
    ~TypeDispatcher() = default;

    TypeDispatcher(const TypeDispatcher&) = delete;
    TypeDispatcher& operator=(const TypeDispatcher&) = delete;

    // Downcasts the ScriptWrappable to the given template parameter type or
    // nullptr if the ScriptWrappable doesn't implement the given type. The
    // inheritance is checked with WrapperTypeInfo, i.e. the check is based on
    // the IDL definitions in *.idl files, not based on C++ class inheritance.
    template <typename T>
    T* DowncastTo() {
      if (wrapper_type_info_->IsSubclass(T::GetStaticWrapperTypeInfo()))
        return static_cast<T*>(script_wrappable_);
      return nullptr;
    }

    // Downcasts the ScriptWrappable to the given template parameter type iff
    // the ScriptWrappable implements the type as the most derived class (i.e.
    // the ScriptWrappable does _not_ implement a subtype of the given type).
    // Otherwise, returns nullptr. The inheritance is checked with
    // WrapperTypeInfo, i.e. the check is based on the IDL definitions in *.idl
    // files, not based on C++ class inheritance.
    template <typename T>
    T* ToMostDerived() {
      if (wrapper_type_info_ == T::GetStaticWrapperTypeInfo())
        return static_cast<T*>(script_wrappable_);
      return nullptr;
    }

   private:
    ScriptWrappable* script_wrappable_ = nullptr;
    const WrapperTypeInfo* wrapper_type_info_ = nullptr;
  };

  ScriptWrappable(const ScriptWrappable&) = delete;
  ScriptWrappable& operator=(const ScriptWrappable&) = delete;
  ~ScriptWrappable() override = default;

  // The following methods may override lifetime of ScriptWrappable objects when
  // needed. In particular if `HasPendingActivity()` returns true *and* the
  // child type also inherits from `ActiveScriptWrappable`, the objects will not
  // be reclaimed by the GC, even if they are otherwise unreachable.
  //
  // Note: These methods are queried during garbage collection and *must not*
  // allocate any new objects.
  virtual bool HasPendingActivity() const { return false; }

  const char* NameInHeapSnapshot() const override;

  virtual void Trace(Visitor*) const;

  // Downcasts this instance to the given template parameter type or nullptr if
  // this instance doesn't implement the given type. The inheritance is checked
  // with WrapperTypeInfo, i.e. the check is based on the IDL definitions in
  // *.idl files, not based on C++ class inheritance.
  template <typename T>
  T* DowncastTo() {
    if (GetWrapperTypeInfo()->IsSubclass(T::GetStaticWrapperTypeInfo()))
      return static_cast<T*>(this);
    return nullptr;
  }

  // Downcasts this instance to the given template parameter type iff this
  // instance implements the type as the most derived class (i.e. this instance
  // does _not_ implement a subtype of the given type). Otherwise, returns
  // nullptr. The inheritance is checked with WrapperTypeInfo, i.e. the check is
  // based on the IDL definitions in *.idl files, not based on C++ class
  // inheritance.
  template <typename T>
  T* ToMostDerived() {
    if (GetWrapperTypeInfo() == T::GetStaticWrapperTypeInfo())
      return static_cast<T*>(this);
    return nullptr;
  }

  template <typename T>
  T* ToImpl() {  // DEPRECATED
    // All ScriptWrappables are managed by the Blink GC heap; check that
    // |T| is a garbage collected type.
    static_assert(
        sizeof(T) && WTF::IsGarbageCollectedType<T>::value,
        "Classes implementing ScriptWrappable must be garbage collected.");

    // Check if T* is castable to ScriptWrappable*, which means T doesn't
    // have two or more ScriptWrappable as superclasses. If T has two
    // ScriptWrappable as superclasses, conversions from T* to
    // ScriptWrappable* are ambiguous.
    static_assert(!static_cast<ScriptWrappable*>(static_cast<T*>(nullptr)),
                  "Class T must not have two or more ScriptWrappable as its "
                  "superclasses.");

    return static_cast<T*>(this);
  }

  // Returns the WrapperTypeInfo of the instance.
  //
  // This method must be overridden by DEFINE_WRAPPERTYPEINFO macro.
  virtual const WrapperTypeInfo* GetWrapperTypeInfo() const = 0;

  // Creates and returns a new wrapper object.
  virtual v8::MaybeLocal<v8::Value> Wrap(ScriptState*);

  // Associates the instance with the given |wrapper| if this instance is not
  // yet associated with any wrapper.  Returns the wrapper already associated
  // or |wrapper| if not yet associated.
  // The caller should always use the returned value rather than |wrapper|.
  [[nodiscard]] virtual v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper);

  // Associates this instance with the given |wrapper| if this instance is not
  // yet associated with any wrapper.  Returns true if the given wrapper is
  // associated with this instance, or false if this instance is already
  // associated with a wrapper.  In the latter case, |wrapper| will be updated
  // to the existing wrapper.
  [[nodiscard]] bool SetWrapper(v8::Isolate* isolate,
                                const WrapperTypeInfo* wrapper_type_info,
                                v8::Local<v8::Object>& wrapper) {
    DCHECK(!wrapper.IsEmpty());
    if (UNLIKELY(ContainsWrapper())) {
      wrapper = MainWorldWrapper(isolate);
      return false;
    }
    main_world_wrapper_.Reset(isolate, wrapper);
    DCHECK(ContainsWrapper());
    wrapper_type_info->ConfigureWrapper(&main_world_wrapper_);
    return true;
  }

  bool IsEqualTo(const v8::Local<v8::Object>& other) const {
    return main_world_wrapper_ == other;
  }

  bool SetReturnValue(v8::ReturnValue<v8::Value> return_value) {
    return_value.Set(main_world_wrapper_);
    return ContainsWrapper();
  }

  bool ContainsWrapper() const { return !main_world_wrapper_.IsEmpty(); }

 protected:
  ScriptWrappable() = default;

 private:
  v8::Local<v8::Object> MainWorldWrapper(v8::Isolate* isolate) const {
    return main_world_wrapper_.Get(isolate);
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
};

inline bool ScriptWrappable::UnsetMainWorldWrapperIfSet(
    const v8::TracedReference<v8::Object>& handle) {
  if (main_world_wrapper_ == handle) {
    main_world_wrapper_.Reset();
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
  static const WrapperTypeInfo* GetStaticWrapperTypeInfo() {   \
    return &wrapper_type_info_;                                \
  }                                                            \
                                                               \
 private:                                                      \
  static const WrapperTypeInfo& wrapper_type_info_

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SCRIPT_WRAPPABLE_H_
