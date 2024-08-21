// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

namespace {

bool DoesExceedSizeLimitSlow(v8::Isolate* isolate,
                             ExceptionState& exception_state) {
  if (base::FeatureList::IsEnabled(
          features::kDisableArrayBufferSizeLimitsForTesting)) {
    return false;
  }

  UseCounter::Count(ExecutionContext::From(isolate->GetCurrentContext()),
                    WebFeature::kArrayBufferTooBigForWebAPI);
  exception_state.ThrowRangeError(
      "The ArrayBuffer/ArrayBufferView size exceeds the supported range.");
  return true;
}

// Throws a RangeError and returns true if the given byte_length exceeds the
// size limit.
//
// TODO(crbug.com/1201109): Remove check once Blink can handle bigger sizes.
inline bool DoesExceedSizeLimit(v8::Isolate* isolate,
                                size_t byte_length,
                                ExceptionState& exception_state) {
  if (byte_length <= ::partition_alloc::MaxDirectMapped()) [[likely]] {
    return false;
  }

  return DoesExceedSizeLimitSlow(isolate, exception_state);
}

enum class Nullablity {
  kIsNotNullable,
  kIsNullable,
};

enum class BufferSizeCheck {
  kCheck,
  kDoNotCheck,
};

enum class ResizableAllowance { kDisallowResizable, kAllowResizable };

// The basic recipe of NativeValueTraits<T>::NativeValue function
// implementation for buffer source types.
template <typename RecipeTrait,
          auto (*ToBlinkValue)(v8::Isolate*, v8::Local<v8::Value>),
          Nullablity nullablity,
          BufferSizeCheck buffer_size_check,
          ResizableAllowance allow_resizable,
          typename ScriptWrappableOrBufferSourceTypeName,
          bool (*IsSharedBuffer)(v8::Local<v8::Value>) = nullptr>
auto NativeValueImpl(v8::Isolate* isolate,
                     v8::Local<v8::Value> value,
                     ExceptionState& exception_state) {
  const char* buffer_source_type_name = nullptr;
  if constexpr (std::is_base_of_v<ScriptWrappable,
                                  ScriptWrappableOrBufferSourceTypeName>) {
    buffer_source_type_name =
        ScriptWrappableOrBufferSourceTypeName::GetStaticWrapperTypeInfo()
            ->interface_name;
  } else {
    buffer_source_type_name = ScriptWrappableOrBufferSourceTypeName::GetName();
  }

  auto blink_value = ToBlinkValue(isolate, value);
  if (RecipeTrait::IsNonNull(blink_value)) [[likely]] {
    if constexpr (allow_resizable == ResizableAllowance::kDisallowResizable) {
      if (RecipeTrait::IsResizable(blink_value)) {
        exception_state.ThrowTypeError(
            ExceptionMessages::ResizableArrayBufferNotAllowed(
                buffer_source_type_name));
        return RecipeTrait::NullValue();
      }
    }

    if constexpr (buffer_size_check == BufferSizeCheck::kCheck) {
      if (DoesExceedSizeLimit(isolate, RecipeTrait::ByteLength(blink_value),
                              exception_state)) {
        return RecipeTrait::NullValue();
      }
    }

    return RecipeTrait::ToReturnType(blink_value);
  }

  if constexpr (nullablity == Nullablity::kIsNullable) {
    if (value->IsNullOrUndefined()) [[likely]] {
      return RecipeTrait::NullValue();
    }
  }

  if constexpr (IsSharedBuffer != nullptr) {
    if (IsSharedBuffer(value)) {
      exception_state.ThrowTypeError(
          ExceptionMessages::SharedArrayBufferNotAllowed(
              buffer_source_type_name));
      return RecipeTrait::NullValue();
    }
  }

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue(buffer_source_type_name));
  return RecipeTrait::NullValue();
}

// The basic recipe of NativeValueTraits<T>::ArgumentValue function
// implementation for buffer source types.
template <typename RecipeTrait,
          auto (*ToBlinkValue)(v8::Isolate*, v8::Local<v8::Value>),
          Nullablity nullablity,
          BufferSizeCheck buffer_size_check,
          ResizableAllowance allow_resizable,
          typename ScriptWrappableOrBufferSourceTypeName,
          bool (*IsSharedBuffer)(v8::Local<v8::Value>) = nullptr>
auto ArgumentValueImpl(v8::Isolate* isolate,
                       int argument_index,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
  const char* buffer_source_type_name = nullptr;
  if constexpr (std::is_base_of_v<ScriptWrappable,
                                  ScriptWrappableOrBufferSourceTypeName>) {
    buffer_source_type_name =
        ScriptWrappableOrBufferSourceTypeName::GetStaticWrapperTypeInfo()
            ->interface_name;
  } else {
    buffer_source_type_name = ScriptWrappableOrBufferSourceTypeName::GetName();
  }

  auto blink_value = ToBlinkValue(isolate, value);
  if (RecipeTrait::IsNonNull(blink_value)) [[likely]] {
    if constexpr (allow_resizable == ResizableAllowance::kDisallowResizable) {
      if (RecipeTrait::IsResizable(blink_value)) {
        exception_state.ThrowTypeError(
            ExceptionMessages::ResizableArrayBufferNotAllowed(
                buffer_source_type_name));
        return RecipeTrait::NullValue();
      }
    }

    if constexpr (buffer_size_check == BufferSizeCheck::kCheck) {
      if (DoesExceedSizeLimit(isolate, RecipeTrait::ByteLength(blink_value),
                              exception_state)) {
        return RecipeTrait::NullValue();
      }
    }

    return RecipeTrait::ToReturnType(blink_value);
  }

  if constexpr (nullablity == Nullablity::kIsNullable) {
    if (value->IsNullOrUndefined()) [[likely]] {
      return RecipeTrait::NullValue();
    }
  }

  if constexpr (IsSharedBuffer != nullptr) {
    if (IsSharedBuffer(value)) {
      exception_state.ThrowTypeError(
          ExceptionMessages::SharedArrayBufferNotAllowed(
              buffer_source_type_name));
      return RecipeTrait::NullValue();
    }
  }

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, buffer_source_type_name));
  return RecipeTrait::NullValue();
}

// ABVTrait implementation for type parameterization purposes

template <typename T>
struct ABVTrait;  // ABV = ArrayBufferView

template <typename DOMViewTypeArg,
          typename V8ViewTypeArg,
          bool (v8::Value::*IsV8ViewTypeMemFunc)() const>
struct ABVTraitImpl {
  using DOMViewType = DOMViewTypeArg;
  using V8ViewType = V8ViewTypeArg;

  static DOMViewType* CreateDOMViewType(DOMArrayBufferBase* blink_buffer,
                                        v8::Local<V8ViewType> v8_view) {
    return DOMViewType::Create(blink_buffer, v8_view->ByteOffset(),
                               v8_view->Length());
  }
  static bool IsV8ViewType(v8::Local<v8::Value> value) {
    return ((*value)->*IsV8ViewTypeMemFunc)();
  }
  static bool IsShared(v8::Local<v8::Value> value) {
    return IsV8ViewType(value) &&
           value.As<V8ViewType>()->Buffer()->IsSharedArrayBuffer();
  }
};

#define DEFINE_ABV_TRAIT(name)                                      \
  template <>                                                       \
  struct ABVTrait<DOM##name>                                        \
      : ABVTraitImpl<DOM##name, v8::name, &v8::Value::Is##name> {};

DEFINE_ABV_TRAIT(ArrayBufferView)
DEFINE_ABV_TRAIT(Int8Array)
DEFINE_ABV_TRAIT(Int16Array)
DEFINE_ABV_TRAIT(Int32Array)
DEFINE_ABV_TRAIT(Uint8Array)
DEFINE_ABV_TRAIT(Uint8ClampedArray)
DEFINE_ABV_TRAIT(Uint16Array)
DEFINE_ABV_TRAIT(Uint32Array)
DEFINE_ABV_TRAIT(BigInt64Array)
DEFINE_ABV_TRAIT(BigUint64Array)
DEFINE_ABV_TRAIT(Float16Array)
DEFINE_ABV_TRAIT(Float32Array)
DEFINE_ABV_TRAIT(Float64Array)
#undef DEFINE_ABV_TRAIT

template <>
struct ABVTrait<DOMDataView>
    : ABVTraitImpl<DOMDataView, v8::DataView, &v8::Value::IsDataView> {
  static DOMViewType* CreateDOMViewType(DOMArrayBufferBase* blink_buffer,
                                        v8::Local<V8ViewType> v8_view) {
    return DOMViewType::Create(blink_buffer, v8_view->ByteOffset(),
                               v8_view->ByteLength());
  }
};

// RecipeTrait implementation for the recipe functions

template <typename T, typename unused = void>
struct RecipeTrait {
  static bool IsNonNull(const T* buffer_view) { return buffer_view; }
  static T* NullValue() { return nullptr; }
  static T* ToReturnType(T* buffer_view) { return buffer_view; }
  static size_t ByteLength(const T* buffer_view) {
    return buffer_view->byteLength();
  }
  static bool IsResizable(const T* buffer_view) {
    return buffer_view->BufferBase()->IsResizableByUserJavaScript();
  }
};

template <typename T>
struct RecipeTrait<T,
                   std::enable_if_t<std::is_base_of_v<DOMArrayBufferBase, T>>> {
  static bool IsNonNull(const T* buffer) { return buffer; }
  static T* NullValue() { return nullptr; }
  static T* ToReturnType(T* buffer) { return buffer; }
  static size_t ByteLength(const T* buffer) { return buffer->ByteLength(); }
  static bool IsResizable(const T* buffer) {
    return buffer->IsResizableByUserJavaScript();
  }
};

template <typename T>
struct RecipeTrait<NotShared<T>, void> : public RecipeTrait<T> {
  static NotShared<T> NullValue() { return NotShared<T>(); }
  static NotShared<T> ToReturnType(T* buffer) { return NotShared<T>(buffer); }
};

template <typename T>
struct RecipeTrait<MaybeShared<T>, void> : public RecipeTrait<T> {
  static MaybeShared<T> NullValue() { return MaybeShared<T>(); }
  static MaybeShared<T> ToReturnType(T* buffer) {
    return MaybeShared<T>(buffer);
  }
};

// ToBlinkValue implementation for the recipe functions

DOMArrayBuffer* ToDOMArrayBuffer(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value) {
  if (!value->IsArrayBuffer()) [[unlikely]] {
    return nullptr;
  }

  v8::Local<v8::ArrayBuffer> v8_array_buffer = value.As<v8::ArrayBuffer>();
  if (auto* array_buffer =
          ToScriptWrappable<DOMArrayBuffer>(isolate, v8_array_buffer)) {
    return array_buffer;
  }

  // Transfer the ownership of the allocated memory to a DOMArrayBuffer without
  // copying.
  ArrayBufferContents contents(v8_array_buffer->GetBackingStore());
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);
  v8::Local<v8::Object> wrapper = array_buffer->AssociateWithWrapper(
      isolate, array_buffer->GetWrapperTypeInfo(), v8_array_buffer);
  DCHECK(wrapper == v8_array_buffer);
  return array_buffer;
}

DOMSharedArrayBuffer* ToDOMSharedArrayBuffer(v8::Isolate* isolate,
                                             v8::Local<v8::Value> value) {
  if (!value->IsSharedArrayBuffer()) [[unlikely]] {
    return nullptr;
  }

  v8::Local<v8::SharedArrayBuffer> v8_shared_array_buffer =
      value.As<v8::SharedArrayBuffer>();
  if (auto* shared_array_buffer = ToScriptWrappable<DOMSharedArrayBuffer>(
          isolate, v8_shared_array_buffer)) {
    return shared_array_buffer;
  }

  // Transfer the ownership of the allocated memory to a DOMArrayBuffer without
  // copying.
  ArrayBufferContents contents(v8_shared_array_buffer->GetBackingStore());
  DOMSharedArrayBuffer* shared_array_buffer =
      DOMSharedArrayBuffer::Create(contents);
  v8::Local<v8::Object> wrapper = shared_array_buffer->AssociateWithWrapper(
      isolate, shared_array_buffer->GetWrapperTypeInfo(),
      v8_shared_array_buffer);
  DCHECK(wrapper == v8_shared_array_buffer);
  return shared_array_buffer;
}

DOMArrayBufferBase* ToDOMArrayBufferBase(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value) {
  if (auto* buffer = ToDOMArrayBuffer(isolate, value)) {
    return buffer;
  }
  return ToDOMSharedArrayBuffer(isolate, value);
}

constexpr bool kNotShared = false;
constexpr bool kMaybeShared = true;

template <typename DOMViewType, bool allow_shared>
DOMViewType* ToDOMViewType(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  using Trait = ABVTrait<DOMViewType>;

  if (!Trait::IsV8ViewType(value)) [[unlikely]] {
    return nullptr;
  }

  v8::Local<typename Trait::V8ViewType> v8_view =
      value.As<typename Trait::V8ViewType>();
  if (auto* blink_view = ToScriptWrappable<DOMViewType>(isolate, v8_view)) {
    return blink_view;
  }

  v8::Local<v8::Object> v8_buffer = v8_view->Buffer();
  DOMArrayBufferBase* blink_buffer = nullptr;
  if constexpr (allow_shared) {
    if (v8_buffer->IsArrayBuffer())
      blink_buffer = ToDOMArrayBuffer(isolate, v8_buffer);
    else  // must be IsSharedArrayBuffer()
      blink_buffer = ToDOMSharedArrayBuffer(isolate, v8_buffer);
  } else {
    if (v8_buffer->IsArrayBuffer()) [[likely]] {
      blink_buffer = ToDOMArrayBuffer(isolate, v8_buffer);
    } else {  // must be IsSharedArrayBuffer()
      return nullptr;
    }
  }

  DOMViewType* blink_view = Trait::CreateDOMViewType(blink_buffer, v8_view);
  v8::Local<v8::Object> wrapper = blink_view->AssociateWithWrapper(
      isolate, blink_view->GetWrapperTypeInfo(), v8_view);
  DCHECK(wrapper == v8_view);
  return blink_view;
}

template <bool allow_shared>
DOMArrayBufferView* ToDOMArrayBufferView(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value) {
  if (!value->IsArrayBufferView()) [[unlikely]] {
    return nullptr;
  }

  v8::Local<v8::ArrayBufferView> v8_view = value.As<v8::ArrayBufferView>();
  if (auto* blink_view =
          ToScriptWrappable<DOMArrayBufferView>(isolate, v8_view)) {
    return blink_view;
  }

  if (v8_view->IsInt8Array()) {
    return ToDOMViewType<DOMInt8Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsInt16Array()) {
    return ToDOMViewType<DOMInt16Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsInt32Array()) {
    return ToDOMViewType<DOMInt32Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsUint8Array()) {
    return ToDOMViewType<DOMUint8Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsUint8ClampedArray()) {
    return ToDOMViewType<DOMUint8ClampedArray, allow_shared>(isolate, value);
  }
  if (v8_view->IsUint16Array()) {
    return ToDOMViewType<DOMUint16Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsUint32Array()) {
    return ToDOMViewType<DOMUint32Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsBigInt64Array()) {
    return ToDOMViewType<DOMBigInt64Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsBigUint64Array()) {
    return ToDOMViewType<DOMBigUint64Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsFloat16Array()) {
    return ToDOMViewType<DOMFloat16Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsFloat32Array()) {
    return ToDOMViewType<DOMFloat32Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsFloat64Array()) {
    return ToDOMViewType<DOMFloat64Array, allow_shared>(isolate, value);
  }
  if (v8_view->IsDataView()) {
    return ToDOMViewType<DOMDataView, allow_shared>(isolate, value);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

template <>
DOMArrayBufferView* ToDOMViewType<DOMArrayBufferView, kNotShared>(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  return ToDOMArrayBufferView<kNotShared>(isolate, value);
}

template <>
DOMArrayBufferView* ToDOMViewType<DOMArrayBufferView, kMaybeShared>(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  return ToDOMArrayBufferView<kMaybeShared>(isolate, value);
}

// ScriptWrappableOrBufferSourceTypeName implementation for the recipe functions

struct BufferSourceTypeNameAllowSharedArrayBuffer {
  static constexpr const char* GetName() { return "[AllowShared] ArrayBuffer"; }
};

}  // namespace

// ArrayBuffer

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                         Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable,
                         DOMArrayBuffer>(isolate, value, exception_state);
}

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                           Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable,
                           DOMArrayBuffer>(isolate, argument_index, value,
                                           exception_state);
}

// Nullable ArrayBuffer

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                         Nullablity::kIsNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable,
                         DOMArrayBuffer>(isolate, value, exception_state);
}

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                           Nullablity::kIsNullable, BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable,
                           DOMArrayBuffer>(isolate, argument_index, value,
                                           exception_state);
}

// [AllowResizable] ArrayBuffer

DOMArrayBuffer*
NativeValueTraits<IDLAllowResizable<DOMArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                         Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kAllowResizable, DOMArrayBuffer>(
      isolate, value, exception_state);
}

DOMArrayBuffer*
NativeValueTraits<IDLAllowResizable<DOMArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBuffer>, ToDOMArrayBuffer,
                           Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                           ResizableAllowance::kAllowResizable, DOMArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// SharedArrayBuffer

DOMSharedArrayBuffer* NativeValueTraits<DOMSharedArrayBuffer>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, DOMSharedArrayBuffer>(
      isolate, value, exception_state);
}

DOMSharedArrayBuffer* NativeValueTraits<DOMSharedArrayBuffer>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, DOMSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// Nullable SharedArrayBuffer

DOMSharedArrayBuffer*
NativeValueTraits<IDLNullable<DOMSharedArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, DOMSharedArrayBuffer>(
      isolate, value, exception_state);
}

DOMSharedArrayBuffer*
NativeValueTraits<IDLNullable<DOMSharedArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, DOMSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// [AllowResizable] SharedArrayBuffer

DOMSharedArrayBuffer*
NativeValueTraits<IDLAllowResizable<DOMSharedArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kAllowResizable, DOMSharedArrayBuffer>(
      isolate, value, exception_state);
}

DOMSharedArrayBuffer*
NativeValueTraits<IDLAllowResizable<DOMSharedArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<DOMSharedArrayBuffer>, ToDOMSharedArrayBuffer,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kAllowResizable, DOMSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// [AllowShared] ArrayBuffer

DOMArrayBufferBase* NativeValueTraits<DOMArrayBufferBase>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<DOMArrayBufferBase>, ToDOMArrayBufferBase,
                         Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable,
                         BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, value, exception_state);
}

DOMArrayBufferBase* NativeValueTraits<DOMArrayBufferBase>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBufferBase>,
                           ToDOMArrayBufferBase, Nullablity::kIsNotNullable,
                           BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable,
                           BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// [AllowShared, BufferSourceTypeNoSizeLimit] ArrayBuffer

DOMArrayBufferBase* NativeValueTraits<IDLBufferSourceTypeNoSizeLimit<
    DOMArrayBufferBase>>::ArgumentValue(v8::Isolate* isolate,
                                        int argument_index,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBufferBase>,
                           ToDOMArrayBufferBase, Nullablity::kIsNotNullable,
                           BufferSizeCheck::kDoNotCheck,
                           ResizableAllowance::kDisallowResizable,
                           BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// Nullable [AllowShared] ArrayBuffer

DOMArrayBufferBase*
NativeValueTraits<IDLNullable<DOMArrayBufferBase>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<DOMArrayBufferBase>, ToDOMArrayBufferBase,
                         Nullablity::kIsNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable,
                         BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, value, exception_state);
}

DOMArrayBufferBase*
NativeValueTraits<IDLNullable<DOMArrayBufferBase>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBufferBase>,
                           ToDOMArrayBufferBase, Nullablity::kIsNullable,
                           BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable,
                           BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// Nullable [AllowShared, BufferSourceTypeNoSizeLimit] ArrayBuffer

DOMArrayBufferBase* NativeValueTraits<
    IDLNullable<IDLBufferSourceTypeNoSizeLimit<DOMArrayBufferBase>>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<DOMArrayBufferBase>,
                           ToDOMArrayBufferBase, Nullablity::kIsNullable,
                           BufferSizeCheck::kDoNotCheck,
                           ResizableAllowance::kDisallowResizable,
                           BufferSourceTypeNameAllowSharedArrayBuffer>(
      isolate, argument_index, value, exception_state);
}

// ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
NotShared<T> NativeValueTraits<NotShared<T>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<
      RecipeTrait<NotShared<T>>, ToDOMViewType<T, kNotShared>,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, T, ABVTrait<T>::IsShared>(
      isolate, value, exception_state);
}

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
NotShared<T> NativeValueTraits<NotShared<T>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<NotShared<T>>, ToDOMViewType<T, kNotShared>,
      Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, T, ABVTrait<T>::IsShared>(
      isolate, argument_index, value, exception_state);
}

// [AllowShared] ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T> NativeValueTraits<MaybeShared<T>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<MaybeShared<T>>,
                         ToDOMViewType<T, kMaybeShared>,
                         Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable, T>(
      isolate, value, exception_state);
}

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T> NativeValueTraits<MaybeShared<T>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<MaybeShared<T>>,
                           ToDOMViewType<T, kMaybeShared>,
                           Nullablity::kIsNotNullable, BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable, T>(
      isolate, argument_index, value, exception_state);
}

// [AllowShared, BufferSourceTypeNoSizeLimit] ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T> NativeValueTraits<IDLBufferSourceTypeNoSizeLimit<
    MaybeShared<T>>>::ArgumentValue(v8::Isolate* isolate,
                                    int argument_index,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<MaybeShared<T>>, ToDOMViewType<T, kMaybeShared>,
      Nullablity::kIsNotNullable, BufferSizeCheck::kDoNotCheck,
      ResizableAllowance::kDisallowResizable, T>(isolate, argument_index, value,
                                                 exception_state);
}

// Nullable ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
NotShared<T> NativeValueTraits<IDLNullable<NotShared<T>>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<
      RecipeTrait<NotShared<T>>, ToDOMViewType<T, kNotShared>,
      Nullablity::kIsNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, T, ABVTrait<T>::IsShared>(
      isolate, value, exception_state);
}

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
NotShared<T> NativeValueTraits<IDLNullable<NotShared<T>>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<NotShared<T>>, ToDOMViewType<T, kNotShared>,
      Nullablity::kIsNullable, BufferSizeCheck::kCheck,
      ResizableAllowance::kDisallowResizable, T, ABVTrait<T>::IsShared>(
      isolate, argument_index, value, exception_state);
}

// Nullable [AllowShared] ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T> NativeValueTraits<IDLNullable<MaybeShared<T>>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return NativeValueImpl<RecipeTrait<MaybeShared<T>>,
                         ToDOMViewType<T, kMaybeShared>,
                         Nullablity::kIsNullable, BufferSizeCheck::kCheck,
                         ResizableAllowance::kDisallowResizable, T>(
      isolate, value, exception_state);
}

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T> NativeValueTraits<IDLNullable<MaybeShared<T>>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return ArgumentValueImpl<RecipeTrait<MaybeShared<T>>,
                           ToDOMViewType<T, kMaybeShared>,
                           Nullablity::kIsNullable, BufferSizeCheck::kCheck,
                           ResizableAllowance::kDisallowResizable, T>(
      isolate, argument_index, value, exception_state);
}

// Nullable [AllowShared, BufferSourceTypeNoSizeLimit] ArrayBufferView

template <typename T>
  requires std::derived_from<T, DOMArrayBufferView>
MaybeShared<T>
NativeValueTraits<IDLNullable<IDLBufferSourceTypeNoSizeLimit<MaybeShared<T>>>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  return ArgumentValueImpl<
      RecipeTrait<MaybeShared<T>>, ToDOMViewType<T, kMaybeShared>,
      Nullablity::kIsNullable, BufferSizeCheck::kDoNotCheck,
      ResizableAllowance::kDisallowResizable, T>(isolate, argument_index, value,
                                                 exception_state);
}

#define INSTANTIATE_NVT(type) \
  template struct CORE_EXPORT NativeValueTraits<type>;
// NotShared<T>
INSTANTIATE_NVT(NotShared<DOMArrayBufferView>)
INSTANTIATE_NVT(NotShared<DOMInt8Array>)
INSTANTIATE_NVT(NotShared<DOMInt16Array>)
INSTANTIATE_NVT(NotShared<DOMInt32Array>)
INSTANTIATE_NVT(NotShared<DOMUint8Array>)
INSTANTIATE_NVT(NotShared<DOMUint8ClampedArray>)
INSTANTIATE_NVT(NotShared<DOMUint16Array>)
INSTANTIATE_NVT(NotShared<DOMUint32Array>)
INSTANTIATE_NVT(NotShared<DOMBigInt64Array>)
INSTANTIATE_NVT(NotShared<DOMBigUint64Array>)
INSTANTIATE_NVT(NotShared<DOMFloat32Array>)
INSTANTIATE_NVT(NotShared<DOMFloat64Array>)
INSTANTIATE_NVT(NotShared<DOMDataView>)
// MaybeShared<T>
INSTANTIATE_NVT(MaybeShared<DOMArrayBufferView>)
INSTANTIATE_NVT(MaybeShared<DOMInt8Array>)
INSTANTIATE_NVT(MaybeShared<DOMInt16Array>)
INSTANTIATE_NVT(MaybeShared<DOMInt32Array>)
INSTANTIATE_NVT(MaybeShared<DOMUint8Array>)
INSTANTIATE_NVT(MaybeShared<DOMUint8ClampedArray>)
INSTANTIATE_NVT(MaybeShared<DOMUint16Array>)
INSTANTIATE_NVT(MaybeShared<DOMUint32Array>)
INSTANTIATE_NVT(MaybeShared<DOMBigInt64Array>)
INSTANTIATE_NVT(MaybeShared<DOMBigUint64Array>)
INSTANTIATE_NVT(MaybeShared<DOMFloat32Array>)
INSTANTIATE_NVT(MaybeShared<DOMFloat64Array>)
INSTANTIATE_NVT(MaybeShared<DOMDataView>)
// IDLBufferSourceTypeNoSizeLimit<MaybeShared<T>>
INSTANTIATE_NVT(IDLBufferSourceTypeNoSizeLimit<MaybeShared<DOMArrayBufferView>>)
INSTANTIATE_NVT(IDLBufferSourceTypeNoSizeLimit<MaybeShared<DOMFloat32Array>>)
INSTANTIATE_NVT(IDLBufferSourceTypeNoSizeLimit<MaybeShared<DOMInt32Array>>)
INSTANTIATE_NVT(IDLBufferSourceTypeNoSizeLimit<MaybeShared<DOMUint32Array>>)
// IDLNullable<NotShared<T>>
INSTANTIATE_NVT(IDLNullable<NotShared<DOMArrayBufferView>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMInt8Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMInt16Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMInt32Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMUint8Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMUint8ClampedArray>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMUint16Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMUint32Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMBigInt64Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMBigUint64Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMFloat32Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMFloat64Array>>)
INSTANTIATE_NVT(IDLNullable<NotShared<DOMDataView>>)
// IDLNullable<MaybeShared<T>>
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMArrayBufferView>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMInt8Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMInt16Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMInt32Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMUint8Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMUint8ClampedArray>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMUint16Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMUint32Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMBigInt64Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMBigUint64Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMFloat32Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMFloat64Array>>)
INSTANTIATE_NVT(IDLNullable<MaybeShared<DOMDataView>>)
// IDLNullable<IDLBufferSourceTypeNoSizeLimit<MaybeShared<T>>>
INSTANTIATE_NVT(
    IDLNullable<
        IDLBufferSourceTypeNoSizeLimit<MaybeShared<DOMArrayBufferView>>>)
#undef INSTANTIATE_NVT

}  // namespace blink
