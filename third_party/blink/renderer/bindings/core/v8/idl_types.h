// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_

#include <type_traits>
#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types_base.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ScriptPromise;

// Boolean
struct IDLBoolean final : public IDLBaseHelper<bool> {};

// Integers
struct IDLByte final : public IDLBaseHelper<int8_t> {};
struct IDLOctet final : public IDLBaseHelper<uint8_t> {};
struct IDLShort final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShort final : public IDLBaseHelper<uint16_t> {};
struct IDLLong final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLong final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLong final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLong final : public IDLBaseHelper<uint64_t> {};

// [Clamp] Integers
struct IDLByteClamp final : public IDLBaseHelper<int8_t> {};
struct IDLOctetClamp final : public IDLBaseHelper<uint8_t> {};
struct IDLShortClamp final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShortClamp final : public IDLBaseHelper<uint16_t> {};
struct IDLLongClamp final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLongClamp final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLongClamp final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLongClamp final : public IDLBaseHelper<uint64_t> {};

// [EnforceRange] Integers
struct IDLByteEnforceRange final : public IDLBaseHelper<int8_t> {};
struct IDLOctetEnforceRange final : public IDLBaseHelper<uint8_t> {};
struct IDLShortEnforceRange final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShortEnforceRange final : public IDLBaseHelper<uint16_t> {};
struct IDLLongEnforceRange final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLongEnforceRange final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLongEnforceRange final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLongEnforceRange final : public IDLBaseHelper<uint64_t> {
};

// Strings
// The "Base" classes are always templatized and require users to specify how JS
// null and/or undefined are supposed to be handled.
template <V8StringResourceMode Mode>
struct IDLByteStringBase final : public IDLBaseHelper<String> {};
template <V8StringResourceMode Mode>
struct IDLStringBase final : public IDLBaseHelper<String> {};
template <V8StringResourceMode Mode>
struct IDLUSVStringBase final : public IDLBaseHelper<String> {};

// Define non-template versions of the above for simplicity.
using IDLByteString = IDLByteStringBase<V8StringResourceMode::kDefaultMode>;
using IDLString = IDLStringBase<V8StringResourceMode::kDefaultMode>;
using IDLUSVString = IDLUSVStringBase<V8StringResourceMode::kDefaultMode>;

// Nullable strings
using IDLByteStringOrNull =
    IDLByteStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;
using IDLStringOrNull =
    IDLStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;
using IDLUSVStringOrNull =
    IDLUSVStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;

// [TreatNullAs] Strings
using IDLStringTreatNullAsEmptyString =
    IDLStringBase<V8StringResourceMode::kTreatNullAsEmptyString>;

// Double
struct IDLDouble final : public IDLBaseHelper<double> {};
struct IDLUnrestrictedDouble final : public IDLBaseHelper<double> {};

// Float
struct IDLFloat final : public IDLBaseHelper<float> {};
struct IDLUnrestrictedFloat final : public IDLBaseHelper<float> {};

struct IDLDate final : public IDLBaseHelper<double> {};

// Promise
struct IDLPromise final : public IDLBaseHelper<ScriptPromise> {};

// Sequence
template <typename T>
struct IDLSequence final : public IDLBase {
  using ImplType = VectorOf<typename NativeValueTraits<T>::ImplType>;
};

// Record
template <typename Key, typename Value>
struct IDLRecord final : public IDLBase {
  static_assert(std::is_same<Key, IDLByteString>::value ||
                    std::is_same<Key, IDLString>::value ||
                    std::is_same<Key, IDLUSVString>::value,
                "IDLRecord keys must be of a WebIDL string type");

  using ImplType =
      VectorOfPairs<String, typename NativeValueTraits<Value>::ImplType>;
};

// Nullable (T?).
// https://heycam.github.io/webidl/#idl-nullable-type
// Types without a built-in notion of nullability are mapped to
// base::Optional<T>.
template <typename InnerType, typename = void>
struct IDLNullable final : public IDLBase {
 private:
  using InnerTraits = NativeValueTraits<InnerType>;
  using InnerResultType =
      decltype(InnerTraits::NativeValue(std::declval<v8::Isolate*>(),
                                        v8::Local<v8::Value>(),
                                        std::declval<ExceptionState&>()));

 public:
  using ResultType = base::Optional<std::decay_t<InnerResultType>>;
  using ImplType = ResultType;
  static inline ResultType NullValue() { return base::nullopt; }
};
template <typename InnerType>
struct IDLNullable<InnerType,
                   decltype(void(NativeValueTraits<InnerType>::NullValue()))>
    final : public IDLBase {
 private:
  using InnerTraits = NativeValueTraits<InnerType>;
  using InnerResultType =
      decltype(InnerTraits::NativeValue(std::declval<v8::Isolate*>(),
                                        v8::Local<v8::Value>(),
                                        std::declval<ExceptionState&>()));

 public:
  using ResultType = InnerResultType;
  using ImplType = typename InnerTraits::ImplType;
  static inline ResultType NullValue() { return InnerTraits::NullValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_
