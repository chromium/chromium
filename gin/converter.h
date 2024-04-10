// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_CONVERTER_H_
#define GIN_CONVERTER_H_

#include <stdint.h>

#include <concepts>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/location.h"
#include "base/notreached.h"
#include "gin/gin_export.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-source-location.h"

namespace base {
class TimeTicks;
}

namespace gin {

template<typename KeyType>
bool SetProperty(v8::Isolate* isolate,
                 v8::Local<v8::Object> object,
                 KeyType key,
                 v8::Local<v8::Value> value) {
  auto maybe =
      object->DefineOwnProperty(isolate->GetCurrentContext(), key, value);
  return !maybe.IsNothing() && maybe.FromJust();
}

template<typename T, typename Enable = void>
struct Converter {};

namespace internal {

template <typename T>
concept ToV8ReturnsMaybe = requires(v8::Isolate* isolate, T value) {
  {
    Converter<T>::ToV8(isolate, value)
  } -> std::same_as<v8::MaybeLocal<v8::Value>>;
};

}  // namespace internal

template<>
struct GIN_EXPORT Converter<bool> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    bool val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     bool* out);
};

template<>
struct GIN_EXPORT Converter<int32_t> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    int32_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     int32_t* out);
};

template<>
struct GIN_EXPORT Converter<uint32_t> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    uint32_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     uint32_t* out);
};

template<>
struct GIN_EXPORT Converter<int64_t> {
  // Warning: JavaScript cannot represent 64 integers precisely.
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    int64_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     int64_t* out);
};

template<>
struct GIN_EXPORT Converter<uint64_t> {
  // Warning: JavaScript cannot represent 64 integers precisely.
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    uint64_t val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     uint64_t* out);
};

template<>
struct GIN_EXPORT Converter<float> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    float val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     float* out);
};

template<>
struct GIN_EXPORT Converter<double> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    double val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     double* out);
};

template <>
struct GIN_EXPORT Converter<std::string_view> {
  // This crashes when val.size() > v8::String::kMaxLength.
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const std::string_view& val);
  // No conversion out is possible because StringPiece does not contain storage.
};

template<>
struct GIN_EXPORT Converter<std::string> {
  // This crashes when val.size() > v8::String::kMaxLength.
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    const std::string& val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     std::string* out);
};

template <>
struct GIN_EXPORT Converter<std::u16string> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const std::u16string& val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     std::u16string* out);
};

// Converter for C++ TimeTicks to Javascript BigInt (unit: microseconds).
// TimeTicks can't be converted using the existing Converter<int64_t> because
// the target type will be Number and will lose precision.
template <>
struct GIN_EXPORT Converter<base::TimeTicks> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, base::TimeTicks val);
};

template <>
struct GIN_EXPORT Converter<v8::Local<v8::Function>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   v8::Local<v8::Function> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::Function>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Local<v8::Object> > {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Local<v8::Object> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::Object>* out);
};

template <>
struct GIN_EXPORT Converter<v8::Local<v8::Promise>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   v8::Local<v8::Promise> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::Promise>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Local<v8::ArrayBuffer> > {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Local<v8::ArrayBuffer> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::ArrayBuffer>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Local<v8::External> > {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Local<v8::External> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::External>* out);
};

template<>
struct GIN_EXPORT Converter<v8::Local<v8::Value> > {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                    v8::Local<v8::Value> val);
  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::Local<v8::Value>* out);
};

template<typename T>
struct Converter<std::vector<T> > {
  static std::conditional_t<internal::ToV8ReturnsMaybe<T>,
                            v8::MaybeLocal<v8::Value>,
                            v8::Local<v8::Value>>
  ToV8(v8::Isolate* isolate, const std::vector<T>& val) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> result(
        v8::Array::New(isolate, static_cast<int>(val.size())));
    for (uint32_t i = 0; i < val.size(); ++i) {
      v8::MaybeLocal<v8::Value> maybe = Converter<T>::ToV8(isolate, val[i]);
      v8::Local<v8::Value> element;
      if (!maybe.ToLocal(&element))
        return {};
      bool property_created;
      if (!result->CreateDataProperty(context, i, element)
               .To(&property_created) ||
          !property_created) {
        NOTREACHED() << "CreateDataProperty should always succeed here.";
      }
    }
    return result;
  }

  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     std::vector<T>* out) {
    if (!val->IsArray())
      return false;

    std::vector<T> result;
    v8::Local<v8::Array> array(v8::Local<v8::Array>::Cast(val));
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      v8::Local<v8::Value> v8_item;
      if (!array->Get(isolate->GetCurrentContext(), i).ToLocal(&v8_item))
        return false;
      T item;
      if (!Converter<T>::FromV8(isolate, v8_item, &item))
        return false;
      result.push_back(std::move(item));
    }

    out->swap(result);
    return true;
  }
};

template <typename T>
struct Converter<v8::LocalVector<T>> {
  static std::conditional_t<internal::ToV8ReturnsMaybe<v8::Local<T>>,
                            v8::MaybeLocal<v8::Value>,
                            v8::Local<v8::Value>>
  ToV8(v8::Isolate* isolate, const v8::LocalVector<T>& val) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> result(
        v8::Array::New(isolate, static_cast<int>(val.size())));
    for (uint32_t i = 0; i < val.size(); ++i) {
      v8::MaybeLocal<v8::Value> maybe =
          Converter<v8::Local<T>>::ToV8(isolate, val[i]);
      v8::Local<v8::Value> element;
      if (!maybe.ToLocal(&element)) {
        return {};
      }
      bool property_created;
      if (!result->CreateDataProperty(context, i, element)
               .To(&property_created) ||
          !property_created) {
        NOTREACHED() << "CreateDataProperty should always succeed here.";
      }
    }
    return result;
  }

  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     v8::LocalVector<T>* out) {
    if (!val->IsArray()) {
      return false;
    }

    v8::LocalVector<T> result(isolate);
    v8::Local<v8::Array> array(v8::Local<v8::Array>::Cast(val));
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      v8::Local<v8::Value> v8_item;
      if (!array->Get(isolate->GetCurrentContext(), i).ToLocal(&v8_item)) {
        return false;
      }
      v8::Local<T> item;
      if (!Converter<v8::Local<T>>::FromV8(isolate, v8_item, &item)) {
        return false;
      }
      result.push_back(item);
    }

    out->swap(result);
    return true;
  }
};

// Convenience functions that deduce T.
template <typename T>
std::conditional_t<internal::ToV8ReturnsMaybe<T>,
                   v8::MaybeLocal<v8::Value>,
                   v8::Local<v8::Value>>
ConvertToV8(v8::Isolate* isolate, const T& input) {
  return Converter<T>::ToV8(isolate, input);
}

template <typename T>
bool TryConvertToV8(v8::Isolate* isolate,
                    const T& input,
                    v8::Local<v8::Value>* output) {
  if constexpr (internal::ToV8ReturnsMaybe<T>) {
    return ConvertToV8(isolate, input).ToLocal(output);
  } else {
    *output = ConvertToV8(isolate, input);
    return true;
  }
}

// This crashes when input.size() > v8::String::kMaxLength.
GIN_EXPORT inline v8::Local<v8::String> StringToV8(
    v8::Isolate* isolate,
    const std::string_view& input) {
  return ConvertToV8(isolate, input).As<v8::String>();
}

// This crashes when input.size() > v8::String::kMaxLength.
GIN_EXPORT v8::Local<v8::String> StringToSymbol(v8::Isolate* isolate,
                                                const std::string_view& val);

// This crashes when input.size() > v8::String::kMaxLength.
GIN_EXPORT v8::Local<v8::String> StringToSymbol(v8::Isolate* isolate,
                                                const std::u16string_view& val);

GIN_EXPORT base::Location V8ToBaseLocation(const v8::SourceLocation& location);

template<typename T>
bool ConvertFromV8(v8::Isolate* isolate, v8::Local<v8::Value> input,
                   T* result) {
  DCHECK(isolate);
  return Converter<T>::FromV8(isolate, input, result);
}

GIN_EXPORT std::string V8ToString(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value);

}  // namespace gin

#endif  // GIN_CONVERTER_H_
