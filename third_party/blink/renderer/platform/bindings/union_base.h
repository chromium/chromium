// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptState;

namespace bindings {

// UnionBase is the common base class of all the IDL union classes.  Most
// importantly this class provides a way of type dispatching (e.g. overload
// resolutions, SFINAE technique, etc.) so that it's possible to distinguish
// IDL unions from anything else.  Also it provides a common implementation of
// IDL unions.
class PLATFORM_EXPORT UnionBase : public GarbageCollected<UnionBase> {
 public:
  virtual ~UnionBase() = default;

  virtual void Trace(Visitor*) const {}

 protected:
  // Helper function to reduce the binary size of the generated bindings.
  static void ThrowTypeErrorNotOfType(ExceptionState& exception_state,
                                      const char* expected_type);

  UnionBase() = default;
};

// A class that can be returned from implementation methods to avoid an extra
// heap allocation and extra dispatch over member types by performing an eager
// ToV8() conversion at the creation site.
// Note that methods that always return a single predetermined member type
// can just directly return it and do not need to use this proxy.
template <typename T>
class OptimizedReturnProxy {
  STACK_ALLOCATED();

 public:
  OptimizedReturnProxy() = default;
  OptimizedReturnProxy(const OptimizedReturnProxy& r) = default;
  OptimizedReturnProxy(const OptimizedReturnProxy&& r) = default;
  OptimizedReturnProxy& operator=(const OptimizedReturnProxy& r) = default;

  template <typename MemberType>
    requires std::is_constructible_v<T, MemberType>
  OptimizedReturnProxy(ScriptState* script_state, MemberType&& value)
      : value_(T::DirectToV8(script_state, std::forward<MemberType>(value))) {}

  bool IsNull() const { return value_.IsEmpty(); }
  explicit operator bool() const { return !IsNull(); }
  v8::Local<v8::Value> ToV8() { return value_.ToLocalChecked(); }

 private:
  v8::MaybeLocal<v8::Value> value_;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
