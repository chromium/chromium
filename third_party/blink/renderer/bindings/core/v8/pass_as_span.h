// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_

#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

namespace bindings::internal {

class CORE_EXPORT ByteSpanWithInlineStorage {
  STACK_ALLOCATED();

 public:
  static constexpr size_t kInlineStorageSize = 64;

  ByteSpanWithInlineStorage() = default;
  ByteSpanWithInlineStorage(const ByteSpanWithInlineStorage& r) { *this = r; }

  ByteSpanWithInlineStorage& operator=(const ByteSpanWithInlineStorage& r);

  void Assign(base::span<const uint8_t> span) { span_ = span; }

  // This class allows implicit conversion to span, because it's an internal
  // class tightly coupled to the bindings generator that knows how to use it.
  // Note rvalue conversion is explicitly disabled.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator base::span<const uint8_t>() const& { return span_; }
  operator base::span<const uint8_t>() const&& = delete;
  const base::span<const uint8_t> as_span() const { return span_; }

  base::span<uint8_t, kInlineStorageSize> GetInlineStorage() {
    return inline_storage_;
  }

 private:
  base::span<const uint8_t> span_;
  uint8_t inline_storage_[kInlineStorageSize];
};

template <typename T>
base::span<const uint8_t> GetArrayData(v8::Local<T> array) {
  return base::make_span(reinterpret_cast<const uint8_t*>(array->Data()),
                         array->ByteLength());
}

CORE_EXPORT base::span<const uint8_t> GetViewData(
    v8::Local<v8::ArrayBufferView> view,
    base::span<uint8_t, ByteSpanWithInlineStorage::kInlineStorageSize>
        inline_storage);

template <typename T>
class SpanWithInlineStorage {
  STACK_ALLOCATED();

 public:
  SpanWithInlineStorage() = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator base::span<const T>() const& { return as_span(); }
  operator base::span<const T>() const&& = delete;
  const base::span<const T> as_span() const {
    const base::span<const uint8_t> bytes = bytes_.as_span();
    return base::make_span(reinterpret_cast<const T*>(bytes.data()),
                           bytes.size() / sizeof(T));
  }

  void Assign(base::span<const uint8_t> span) { bytes_.Assign(span); }
  base::span<uint8_t, ByteSpanWithInlineStorage::kInlineStorageSize>
  GetInlineStorage() {
    return bytes_.GetInlineStorage();
  }

 private:
  ByteSpanWithInlineStorage bytes_;
};

template <typename T>
struct TypedArrayElementTraits {};

#define DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(type, func)      \
  template <>                                              \
  struct TypedArrayElementTraits<type> {                   \
    static bool IsViewOfType(v8::Local<v8::Value> value) { \
      return value->func();                                \
    }                                                      \
  }

DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(int8_t, IsInt8Array);
// Note Uint8 array is special case due to need to account for
// Uint8 clamped array, so not declared here.
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(int16_t, IsInt16Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(uint16_t, IsUint16Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(int32_t, IsInt32Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(uint32_t, IsUint32Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(int64_t, IsBigInt64Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(uint64_t, IsBigUint64Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(float, IsFloat32Array);
DEFINE_TYPED_ARRAY_ELEMENT_TRAITS(double, IsFloat64Array);

template <>
struct TypedArrayElementTraits<uint8_t> {
  static bool IsViewOfType(v8::Local<v8::Value> value) {
    return value->IsUint8Array() || value->IsUint8ClampedArray();
  }
};

}  // namespace bindings::internal

// This is a marker class for differentiating [PassAsSpan] argument conversions.
// The actual type returned is `SpanWithInlineStorage`, however, unlike the
// returned type, the marker carries additional information for conversion
// (whether shared array buffers should be allowed, whether a typed array
// is expected etc).
struct PassAsSpanMarkerBase {
  enum Flags {
    kNone,
    kAllowShared = 1 << 0,
  };
};

constexpr PassAsSpanMarkerBase::Flags operator|(PassAsSpanMarkerBase::Flags a,
                                                PassAsSpanMarkerBase::Flags b) {
  return static_cast<PassAsSpanMarkerBase::Flags>(static_cast<int>(a) |
                                                  static_cast<int>(b));
}

template <PassAsSpanMarkerBase::Flags flags =
              PassAsSpanMarkerBase::Flags::kNone,
          typename T = void>
struct PassAsSpan : public PassAsSpanMarkerBase {
  static constexpr bool allow_shared = flags & Flags::kAllowShared;
  static constexpr bool is_typed = !std::is_same_v<T, void>;
  using ElementType = T;
  using ReturnType =
      std::conditional_t<is_typed,
                         bindings::internal::SpanWithInlineStorage<T>,
                         bindings::internal::ByteSpanWithInlineStorage>;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_
