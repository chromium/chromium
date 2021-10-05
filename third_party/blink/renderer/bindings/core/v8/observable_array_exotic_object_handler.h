// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace blink {

namespace bindings {

// ObservableArrayExoticObjectHandler implements a handler object which is part
// of an observable array exotic object.
//
//   let observable_array_exotic_object = new Proxy(target, handler);
// where
//   target = observable_array_backing_list_object
//   handler = v8::Object that has a set of trap functions implemented in
//       ObservableArrayExoticObjectHandler.
//
// https://heycam.github.io/webidl/#creating-an-observable-array-exotic-object
//
// Implementation notes:
// - v8::Value::ToArrayIndex returns an empty handle if the conversion fails
//   without throwing an exception despite that the return type is
//   v8::MaybeLocal.
// - The case of 'the property == an array index' has priority over the case of
//   'the property == "length"' in order to optimize the former case, which is
//   likely to happen more frequently.
template <typename BackingListWrappable, typename ElementIdlType>
class ObservableArrayExoticObjectHandler {
  STATIC_ONLY(ObservableArrayExoticObjectHandler);

 public:
  // https://heycam.github.io/webidl/#es-observable-array-defineProperty
  static void TrapDefineProperty(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      // TODO(yukishiino): Implement this case appropriately.
      (void)backing_list;
      V8SetReturnValue(info, false);
      return;
    }

    DCHECK(v8_property->IsString());
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      // TODO(yukishiino): Implement this case appropriately.
      V8SetReturnValue(info, false);
      return;
    }
  }

  // https://heycam.github.io/webidl/#es-observable-array-deleteProperty
  static void TrapDeleteProperty(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      uint32_t index = v8_index->Value();
      if (!(backing_list.size() != 0 && index == backing_list.size() - 1)) {
        V8SetReturnValue(info, false);
        return;
      }
      ScriptState* script_state = ScriptState::From(current_context);
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kIndexedPropertyDelete,
          backing_list.ObservableArrayNameInIDL());
      if (!RunDeleteAlgorithm(script_state, backing_list, index,
                              exception_state)) {
        return;
      }
      backing_list.pop_back();
      V8SetReturnValue(info, true);
      return;
    }

    DCHECK(v8_property->IsString());
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      V8SetReturnValue(info, false);
      return;
    }
  }

  // https://heycam.github.io/webidl/#es-observable-array-get
  static void TrapGet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      uint32_t index = v8_index->Value();
      if (backing_list.size() <= index) {
        V8SetReturnValue(info, v8::Undefined(isolate));
        return;
      }
      v8::Local<v8::Value> v8_element;
      ScriptState* script_state = ScriptState::From(current_context);
      if (!ToV8Traits<ElementIdlType>::ToV8(script_state, backing_list[index])
               .ToLocal(&v8_element)) {
        return;
      }
      V8SetReturnValue(info, v8_element);
      return;
    }

    DCHECK(v8_property->IsString());
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      V8SetReturnValue(info, backing_list.size());
      return;
    }
  }

  // https://heycam.github.io/webidl/#es-observable-array-getOwnPropertyDescriptor
  static void TrapGetOwnPropertyDescriptor(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      uint32_t index = v8_index->Value();
      if (backing_list.size() <= index) {
        V8SetReturnValue(info, v8::Undefined(isolate));
        return;
      }
      v8::Local<v8::Value> v8_element;
      ScriptState* script_state = ScriptState::From(current_context);
      if (!ToV8Traits<ElementIdlType>::ToV8(script_state, backing_list[index])
               .ToLocal(&v8_element)) {
        return;
      }
      v8::PropertyDescriptor prop_desc(v8_element, true);
      prop_desc.set_configurable(true);
      prop_desc.set_enumerable(true);
      V8SetReturnValue(info, prop_desc);
      return;
    }

    DCHECK(v8_property->IsString());
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      v8::PropertyDescriptor prop_desc(
          v8::Integer::NewFromUnsigned(isolate, backing_list.size()), true);
      prop_desc.set_configurable(true);
      prop_desc.set_enumerable(true);
      V8SetReturnValue(info, prop_desc);
      return;
    }
  }

  // https://heycam.github.io/webidl/#es-observable-array-has
  static void TrapHas(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      uint32_t index = v8_index->Value();
      V8SetReturnValue(info, index < backing_list.size());
      return;
    }

    DCHECK(v8_property->IsString());
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      V8SetReturnValue(info, true);
      return;
    }
  }

  // https://heycam.github.io/webidl/#es-observable-array-ownKeys
  static void TrapOwnKeys(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    const auto& v8_target = info[0];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Array> own_keys = v8::Array::New(isolate);
    for (uint32_t index = 0; index < backing_list.size(); ++index) {
      v8::Local<v8::String> key;
      if (!v8::Integer::NewFromUnsigned(isolate, index)
               ->ToString(current_context)
               .ToLocal(&key)) {
        return;
      }
      bool is_created = false;
      if (!own_keys->CreateDataProperty(current_context, index, key)
               .To(&is_created)) {
        return;
      }
      DCHECK(is_created);
    }
    uint32_t own_keys_index = backing_list.size();

    v8::Local<v8::Array> own_props;
    if (!v8_target.As<v8::Object>()
             ->GetOwnPropertyNames(current_context)
             .ToLocal(&own_props)) {
      return;
    }
    uint32_t own_props_length = own_props->Length();
    for (uint32_t index = 0; index < own_props_length; ++index) {
      v8::Local<v8::Value> prop_name;
      if (!own_props->Get(current_context, index).ToLocal(&prop_name))
        return;
      bool is_created = false;
      if (!own_keys
               ->CreateDataProperty(current_context, own_keys_index++,
                                    prop_name)
               .To(&is_created)) {
        return;
      }
      DCHECK(is_created);
    }

    V8SetReturnValue(info, own_keys);
  }

  // https://heycam.github.io/webidl/#es-observable-array-preventExtensions
  static void TrapPreventExtensions(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    V8SetReturnValue(info, false);
  }

  // https://heycam.github.io/webidl/#es-observable-array-set
  static void TrapSet(const v8::FunctionCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    ScriptState* script_state = ScriptState::From(current_context);
    const auto& v8_target = info[0];
    const auto& v8_property = info[1];
    const auto& v8_value = info[2];
    auto& backing_list = ToWrappableUnsafe(v8_target);

    v8::Local<v8::Uint32> v8_index;
    if (v8_property->ToArrayIndex(current_context).ToLocal(&v8_index)) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kIndexedPropertySet,
          backing_list.ObservableArrayNameInIDL());
      uint32_t index = v8_index->Value();
      bool result = DoSetTheIndexedValue(script_state, backing_list, index,
                                         v8_value, exception_state);
      V8SetReturnValue(info, result);
      return;
    }

    DCHECK(v8_property->IsString());
    v8::Local<v8::Uint32> v8_length;
    if (v8_property.As<v8::String>()->StringEquals(
            V8AtomicString(isolate, "length"))) {
      ExceptionState exception_state(
          isolate, ExceptionContext::Context::kAttributeSet,
          backing_list.ObservableArrayNameInIDL(), "length");
      if (!v8_value->ToArrayIndex(current_context).ToLocal(&v8_length)) {
        exception_state.ThrowRangeError("The provided value is invalid.");
        return;
      }
      uint32_t length = v8_length->Value();
      bool result =
          DoSetTheLength(script_state, backing_list, length, exception_state);
      V8SetReturnValue(info, result);
      return;
    }
  }

 private:
  static BackingListWrappable& ToWrappableUnsafe(v8::Local<v8::Value> target) {
    return *ToScriptWrappable(target.As<v8::Object>())
                ->ToImpl<BackingListWrappable>();
  }

  // https://heycam.github.io/webidl/#observable-array-exotic-object-set-the-length
  static bool DoSetTheLength(ScriptState* script_state,
                             BackingListWrappable& backing_list,
                             uint32_t length,
                             ExceptionState& exception_state) {
    if (backing_list.size() < length)
      return false;

    if (backing_list.size() == 0)
      return true;

    uint32_t index_to_delete = backing_list.size() - 1;
    while (length <= index_to_delete) {
      if (!RunDeleteAlgorithm(script_state, backing_list, index_to_delete,
                              exception_state)) {
        return false;
      }

      backing_list.pop_back();
      if (index_to_delete == 0)
        break;
      --index_to_delete;
    }
    return true;
  }

  // https://heycam.github.io/webidl/#observable-array-exotic-object-set-the-indexed-value
  static bool DoSetTheIndexedValue(ScriptState* script_state,
                                   BackingListWrappable& backing_list,
                                   uint32_t index,
                                   v8::Local<v8::Value> v8_value,
                                   ExceptionState& exception_state) {
    if (backing_list.size() < index)
      return false;

    typename BackingListWrappable::value_type blink_value =
        NativeValueTraits<ElementIdlType>::NativeValue(
            script_state->GetIsolate(), v8_value, exception_state);
    if (exception_state.HadException())
      return false;

    if (index < backing_list.size()) {
      if (!RunDeleteAlgorithm(script_state, backing_list, index,
                              exception_state)) {
        return false;
      }
    }
    if (!RunSetAlgorithm(script_state, backing_list, index, blink_value,
                         exception_state)) {
      return false;
    }

    if (index == backing_list.size())
      backing_list.push_back(std::move(blink_value));
    else
      backing_list[index] = std::move(blink_value);
    return true;
  }

  // https://heycam.github.io/webidl/#observable-array-attribute-set-an-indexed-value
  static bool RunSetAlgorithm(ScriptState* script_state,
                              BackingListWrappable& backing_list,
                              typename BackingListWrappable::size_type index,
                              typename BackingListWrappable::value_type& value,
                              ExceptionState& exception_state) {
    if (!backing_list.set_algorithm_callback_)
      return true;

    (backing_list.GetPlatformObject()->*backing_list.set_algorithm_callback_)(
        script_state, backing_list, index, value, exception_state);
    return !exception_state.HadException();
  }

  // https://heycam.github.io/webidl/#observable-array-attribute-delete-an-indexed-value
  static bool RunDeleteAlgorithm(ScriptState* script_state,
                                 BackingListWrappable& backing_list,
                                 typename BackingListWrappable::size_type index,
                                 ExceptionState& exception_state) {
    if (!backing_list.delete_algorithm_callback_)
      return true;

    (backing_list.GetPlatformObject()
         ->*backing_list.delete_algorithm_callback_)(script_state, backing_list,
                                                     index, exception_state);
    return !exception_state.HadException();
  }
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_OBSERVABLE_ARRAY_EXOTIC_OBJECT_HANDLER_H_
