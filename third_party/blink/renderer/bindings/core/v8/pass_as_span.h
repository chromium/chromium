// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_

#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

namespace internal {

class CORE_EXPORT ByteSpanWithInlineStorage {
  STACK_ALLOCATED();

 public:
  ByteSpanWithInlineStorage() = default;
  ByteSpanWithInlineStorage(const ByteSpanWithInlineStorage& r) { *this = r; }

  ByteSpanWithInlineStorage& operator=(const ByteSpanWithInlineStorage& r);

  template <typename T>
  static ByteSpanWithInlineStorage GetArrayData(v8::Local<T> array) {
    return ByteSpanWithInlineStorage(base::make_span(
        reinterpret_cast<const uint8_t*>(array->Data()), array->ByteLength()));
  }

  static ByteSpanWithInlineStorage GetViewData(
      v8::Local<v8::ArrayBufferView> view);

  // This class allows implicit conversion to span, because it's an internal
  // class tightly coupled to the bindings generator that knows how to use it.
  // Note rvalue conversion is explicitly disabled.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator base::span<const uint8_t>() const& { return span_; }
  operator base::span<const uint8_t>() const&& = delete;
  const base::span<const uint8_t> as_span() const { return span_; }

 private:
  explicit ByteSpanWithInlineStorage(base::span<const uint8_t> span)
      : span_(span) {}
  explicit ByteSpanWithInlineStorage(size_t size)
      : ByteSpanWithInlineStorage(base::make_span(inline_storage_, size)) {
    DCHECK_LE(size, sizeof inline_storage_);
  }

  base::span<const uint8_t> span_;
  uint8_t inline_storage_[64];
};

template <typename T>
class SpanWithInlineStorage {
  STACK_ALLOCATED();

 public:
  SpanWithInlineStorage() = default;

  static SpanWithInlineStorage GetViewData(
      v8::Local<v8::ArrayBufferView> view) {
    return SpanWithInlineStorage(view);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator base::span<const T>() const& { return as_span(); }
  operator base::span<const T>() const&& = delete;
  const base::span<const T> as_span() const {
    const base::span<const uint8_t> bytes = bytes_.as_span();
    return base::make_span(reinterpret_cast<const T*>(bytes.data()),
                           bytes.size() / sizeof(T));
  }

 private:
  explicit SpanWithInlineStorage(v8::Local<v8::ArrayBufferView> view)
      : bytes_(ByteSpanWithInlineStorage::GetViewData(view)) {}
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

}  // namespace internal

// This is a marker class for differentiating [PassAsSpan] argument conversions.
// The actual type returned is `SpanWithInlineStorage`, however, unlike the
// returned type, the marker carries additional information for conversion
// (whether shared array buffers should be allowed, whether a typed array
// is expected etc).
struct PassAsSpanMarkerBase {
  enum class AllowSharedFlag { kAllowShared, kDoNotAllowShared };
};

template <PassAsSpanMarkerBase::AllowSharedFlag AllowShared, typename T = void>
struct PassAsSpan : public PassAsSpanMarkerBase {
  static constexpr bool allow_shared =
      AllowShared == AllowSharedFlag::kAllowShared;
  static constexpr bool is_typed = true;
  using ElementType = T;
  using ReturnType = internal::SpanWithInlineStorage<T>;
};

template <PassAsSpanMarkerBase::AllowSharedFlag AllowShared>
struct PassAsSpan<AllowShared, void> : public PassAsSpanMarkerBase {
  static constexpr bool allow_shared =
      AllowShared == AllowSharedFlag::kAllowShared;
  static constexpr bool is_typed = false;
  using ReturnType = internal::ByteSpanWithInlineStorage;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PASS_AS_SPAN_H_
