// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/typed_flexible_array_buffer_view.h"

namespace blink {

namespace {

DOMArrayBuffer* ToDOMArrayBuffer(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value) {
  if (UNLIKELY(!value->IsArrayBuffer()))
    return nullptr;

  v8::Local<v8::ArrayBuffer> v8_array_buffer = value.As<v8::ArrayBuffer>();
  if (DOMArrayBuffer* array_buffer =
          ToScriptWrappable(v8_array_buffer)->ToImpl<DOMArrayBuffer>()) {
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
  if (UNLIKELY(!value->IsSharedArrayBuffer()))
    return nullptr;

  v8::Local<v8::SharedArrayBuffer> v8_shared_array_buffer =
      value.As<v8::SharedArrayBuffer>();
  if (DOMSharedArrayBuffer* shared_array_buffer =
          ToScriptWrappable(v8_shared_array_buffer)
              ->ToImpl<DOMSharedArrayBuffer>()) {
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
      : ABVTraitImpl<DOM##name, v8::name, &v8::Value::Is##name> {}; \
  template <>                                                       \
  struct ABVTrait<Flexible##name>                                   \
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

constexpr bool kNotShared = false;
constexpr bool kMaybeShared = true;

template <typename DOMViewType, bool allow_shared>
DOMViewType* ToDOMViewType(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  using Trait = ABVTrait<DOMViewType>;

  if (UNLIKELY(!Trait::IsV8ViewType(value)))
    return nullptr;

  v8::Local<typename Trait::V8ViewType> v8_view =
      value.As<typename Trait::V8ViewType>();
  if (DOMViewType* blink_view =
          ToScriptWrappable(v8_view)->template ToImpl<DOMViewType>()) {
    return blink_view;
  }

  v8::Local<v8::Object> v8_buffer = v8_view->Buffer();
  DOMArrayBufferBase* blink_buffer = nullptr;
  if (allow_shared) {
    if (v8_buffer->IsArrayBuffer())
      blink_buffer = ToDOMArrayBuffer(isolate, v8_buffer);
    else  // must be IsSharedArrayBuffer()
      blink_buffer = ToDOMSharedArrayBuffer(isolate, v8_buffer);
  } else {
    if (LIKELY(v8_buffer->IsArrayBuffer()))
      blink_buffer = ToDOMArrayBuffer(isolate, v8_buffer);
    else  // must be IsSharedArrayBuffer()
      return nullptr;
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
  if (UNLIKELY(!value->IsArrayBufferView()))
    return nullptr;

  v8::Local<v8::ArrayBufferView> v8_view = value.As<v8::ArrayBufferView>();
  if (DOMArrayBufferView* blink_view =
          ToScriptWrappable(v8_view)->template ToImpl<DOMArrayBufferView>()) {
    return blink_view;
  }

  if (v8_view->IsInt8Array())
    return ToDOMViewType<DOMInt8Array, allow_shared>(isolate, value);
  if (v8_view->IsInt16Array())
    return ToDOMViewType<DOMInt16Array, allow_shared>(isolate, value);
  if (v8_view->IsInt32Array())
    return ToDOMViewType<DOMInt32Array, allow_shared>(isolate, value);
  if (v8_view->IsUint8Array())
    return ToDOMViewType<DOMUint8Array, allow_shared>(isolate, value);
  if (v8_view->IsUint8ClampedArray())
    return ToDOMViewType<DOMUint8ClampedArray, allow_shared>(isolate, value);
  if (v8_view->IsUint16Array())
    return ToDOMViewType<DOMUint16Array, allow_shared>(isolate, value);
  if (v8_view->IsUint32Array())
    return ToDOMViewType<DOMUint32Array, allow_shared>(isolate, value);
  if (v8_view->IsBigInt64Array())
    return ToDOMViewType<DOMBigInt64Array, allow_shared>(isolate, value);
  if (v8_view->IsBigUint64Array())
    return ToDOMViewType<DOMBigUint64Array, allow_shared>(isolate, value);
  if (v8_view->IsFloat32Array())
    return ToDOMViewType<DOMFloat32Array, allow_shared>(isolate, value);
  if (v8_view->IsFloat64Array())
    return ToDOMViewType<DOMFloat64Array, allow_shared>(isolate, value);
  if (v8_view->IsDataView())
    return ToDOMViewType<DOMDataView, allow_shared>(isolate, value);

  NOTREACHED();
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

}  // namespace

// ArrayBuffer

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("ArrayBuffer"));
  return nullptr;
}

DOMArrayBuffer* NativeValueTraits<DOMArrayBuffer>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "ArrayBuffer"));
  return nullptr;
}

// Nullable ArrayBuffer

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("ArrayBuffer"));
  return nullptr;
}

DOMArrayBuffer* NativeValueTraits<IDLNullable<DOMArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "ArrayBuffer"));
  return nullptr;
}

// SharedArrayBuffer

DOMSharedArrayBuffer* NativeValueTraits<DOMSharedArrayBuffer>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("SharedArrayBuffer"));
  return nullptr;
}

DOMSharedArrayBuffer* NativeValueTraits<DOMSharedArrayBuffer>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, "SharedArrayBuffer"));
  return nullptr;
}

// Nullable SharedArrayBuffer

DOMSharedArrayBuffer*
NativeValueTraits<IDLNullable<DOMSharedArrayBuffer>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("SharedArrayBuffer"));
  return nullptr;
}

DOMSharedArrayBuffer*
NativeValueTraits<IDLNullable<DOMSharedArrayBuffer>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, "SharedArrayBuffer"));
  return nullptr;
}

// [AllowShared] ArrayBuffer

DOMArrayBufferBase* NativeValueTraits<DOMArrayBufferBase>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("[AllowShared] ArrayBuffer"));
  return nullptr;
}

DOMArrayBufferBase* NativeValueTraits<DOMArrayBufferBase>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, "[AllowShared] ArrayBuffer"));
  return nullptr;
}

// Nullable [AllowShared] ArrayBuffer

DOMArrayBufferBase*
NativeValueTraits<IDLNullable<DOMArrayBufferBase>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("[AllowShared] ArrayBuffer"));
  return nullptr;
}

DOMArrayBufferBase*
NativeValueTraits<IDLNullable<DOMArrayBufferBase>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  DOMArrayBuffer* array_buffer = ToDOMArrayBuffer(isolate, value);
  if (LIKELY(array_buffer))
    return array_buffer;

  DOMSharedArrayBuffer* shared_array_buffer =
      ToDOMSharedArrayBuffer(isolate, value);
  if (LIKELY(shared_array_buffer))
    return shared_array_buffer;

  if (LIKELY(value->IsNullOrUndefined()))
    return nullptr;

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, "[AllowShared] ArrayBuffer"));
  return nullptr;
}

// ArrayBufferView

template <typename T>
NotShared<T> NativeValueTraits<
    NotShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    NativeValue(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kNotShared>(isolate, value);
  if (LIKELY(blink_view))
    return NotShared<T>(blink_view);

  if (ABVTrait<T>::IsShared(value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::SharedArrayBufferNotAllowed(
            T::GetStaticWrapperTypeInfo()->interface_name));
    return NotShared<T>();
  }

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      T::GetStaticWrapperTypeInfo()->interface_name));
  return NotShared<T>();
}

template <typename T>
NotShared<T> NativeValueTraits<
    NotShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kNotShared>(isolate, value);
  if (LIKELY(blink_view))
    return NotShared<T>(blink_view);

  if (ABVTrait<T>::IsShared(value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::SharedArrayBufferNotAllowed(
            T::GetStaticWrapperTypeInfo()->interface_name));
    return NotShared<T>();
  }

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, T::GetStaticWrapperTypeInfo()->interface_name));
  return NotShared<T>();
}

// [AllowShared] ArrayBufferView

template <typename T>
MaybeShared<T> NativeValueTraits<
    MaybeShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    NativeValue(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kMaybeShared>(isolate, value);
  if (LIKELY(blink_view))
    return MaybeShared<T>(blink_view);

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      T::GetStaticWrapperTypeInfo()->interface_name));
  return MaybeShared<T>();
}

template <typename T>
MaybeShared<T> NativeValueTraits<
    MaybeShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kMaybeShared>(isolate, value);
  if (LIKELY(blink_view))
    return MaybeShared<T>(blink_view);

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, T::GetStaticWrapperTypeInfo()->interface_name));
  return MaybeShared<T>();
}

// Nullable ArrayBufferView

template <typename T>
NotShared<T> NativeValueTraits<
    IDLNullable<NotShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    NativeValue(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kNotShared>(isolate, value);
  if (LIKELY(blink_view))
    return NotShared<T>(blink_view);

  if (LIKELY(value->IsNullOrUndefined()))
    return NotShared<T>();

  if (ABVTrait<T>::IsShared(value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::SharedArrayBufferNotAllowed(
            T::GetStaticWrapperTypeInfo()->interface_name));
    return NotShared<T>();
  }

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      T::GetStaticWrapperTypeInfo()->interface_name));
  return NotShared<T>();
}

template <typename T>
NotShared<T> NativeValueTraits<
    IDLNullable<NotShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kNotShared>(isolate, value);
  if (LIKELY(blink_view))
    return NotShared<T>(blink_view);

  if (LIKELY(value->IsNullOrUndefined()))
    return NotShared<T>();

  if (ABVTrait<T>::IsShared(value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::SharedArrayBufferNotAllowed(
            T::GetStaticWrapperTypeInfo()->interface_name));
    return NotShared<T>();
  }

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, T::GetStaticWrapperTypeInfo()->interface_name));
  return NotShared<T>();
}

// Nullable [AllowShared] ArrayBufferView

template <typename T>
MaybeShared<T> NativeValueTraits<
    IDLNullable<MaybeShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    NativeValue(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kMaybeShared>(isolate, value);
  if (LIKELY(blink_view))
    return MaybeShared<T>(blink_view);

  if (LIKELY(value->IsNullOrUndefined()))
    return MaybeShared<T>();

  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      T::GetStaticWrapperTypeInfo()->interface_name));
  return MaybeShared<T>();
}

template <typename T>
MaybeShared<T> NativeValueTraits<
    IDLNullable<MaybeShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  T* blink_view = ToDOMViewType<T, kMaybeShared>(isolate, value);
  if (LIKELY(blink_view))
    return MaybeShared<T>(blink_view);

  if (LIKELY(value->IsNullOrUndefined()))
    return MaybeShared<T>();

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, T::GetStaticWrapperTypeInfo()->interface_name));
  return MaybeShared<T>();
}

// [AllowShared, FlexibleArrayBufferView] ArrayBufferView

template <typename T>
T NativeValueTraits<T,
                    typename std::enable_if_t<
                        std::is_base_of<FlexibleArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  if (LIKELY(ABVTrait<T>::IsV8ViewType(value)))
    return T(value.As<typename ABVTrait<T>::V8ViewType>());

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index,
      ABVTrait<T>::DOMViewType::GetStaticWrapperTypeInfo()->interface_name));
  return T();
}

// Nullable [AllowShared, FlexibleArrayBufferView] ArrayBufferView

template <typename T>
T NativeValueTraits<IDLNullable<T>,
                    typename std::enable_if_t<
                        std::is_base_of<FlexibleArrayBufferView, T>::value>>::
    ArgumentValue(v8::Isolate* isolate,
                  int argument_index,
                  v8::Local<v8::Value> value,
                  ExceptionState& exception_state) {
  if (LIKELY(ABVTrait<T>::IsV8ViewType(value)))
    return T(value.As<typename ABVTrait<T>::V8ViewType>());

  if (LIKELY(value->IsNullOrUndefined()))
    return T();

  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index,
      ABVTrait<T>::DOMViewType::GetStaticWrapperTypeInfo()->interface_name));
  return T();
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
// FlexibleArrayBufferView
INSTANTIATE_NVT(FlexibleArrayBufferView)
INSTANTIATE_NVT(FlexibleInt8Array)
INSTANTIATE_NVT(FlexibleInt16Array)
INSTANTIATE_NVT(FlexibleInt32Array)
INSTANTIATE_NVT(FlexibleUint8Array)
INSTANTIATE_NVT(FlexibleUint8ClampedArray)
INSTANTIATE_NVT(FlexibleUint16Array)
INSTANTIATE_NVT(FlexibleUint32Array)
INSTANTIATE_NVT(FlexibleBigInt64Array)
INSTANTIATE_NVT(FlexibleBigUint64Array)
INSTANTIATE_NVT(FlexibleFloat32Array)
INSTANTIATE_NVT(FlexibleFloat64Array)
// IDLNullable<FlexibleArrayBufferView>
INSTANTIATE_NVT(IDLNullable<FlexibleArrayBufferView>)
INSTANTIATE_NVT(IDLNullable<FlexibleInt8Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleInt16Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleInt32Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleUint8Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleUint8ClampedArray>)
INSTANTIATE_NVT(IDLNullable<FlexibleUint16Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleUint32Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleBigInt64Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleBigUint64Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleFloat32Array>)
INSTANTIATE_NVT(IDLNullable<FlexibleFloat64Array>)
#undef INSTANTIATE_NVT

}  // namespace blink
