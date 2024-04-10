// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/converter.h"

#include <stdint.h>

#include <string_view>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-maybe.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-promise.h"
#include "v8/include/v8-value.h"

using v8::ArrayBuffer;
using v8::External;
using v8::Function;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace {

template <typename T, typename U>
bool FromMaybe(Maybe<T> maybe, U* out) {
  if (maybe.IsNothing())
    return false;
  *out = static_cast<U>(maybe.FromJust());
  return true;
}

}  // namespace

namespace gin {

Local<Value> Converter<bool>::ToV8(Isolate* isolate, bool val) {
  return v8::Boolean::New(isolate, val).As<Value>();
}

bool Converter<bool>::FromV8(Isolate* isolate, Local<Value> val, bool* out) {
  *out = val->BooleanValue(isolate);
  // BooleanValue cannot throw.
  return true;
}

Local<Value> Converter<int32_t>::ToV8(Isolate* isolate, int32_t val) {
  return Integer::New(isolate, val).As<Value>();
}

bool Converter<int32_t>::FromV8(Isolate* isolate,
                                Local<Value> val,
                                int32_t* out) {
  if (!val->IsInt32())
    return false;
  *out = val.As<Int32>()->Value();
  return true;
}

Local<Value> Converter<uint32_t>::ToV8(Isolate* isolate, uint32_t val) {
  return Integer::NewFromUnsigned(isolate, val).As<Value>();
}

bool Converter<uint32_t>::FromV8(Isolate* isolate,
                                 Local<Value> val,
                                 uint32_t* out) {
  if (!val->IsUint32())
    return false;
  *out = val.As<Uint32>()->Value();
  return true;
}

Local<Value> Converter<int64_t>::ToV8(Isolate* isolate, int64_t val) {
  return Number::New(isolate, static_cast<double>(val)).As<Value>();
}

bool Converter<int64_t>::FromV8(Isolate* isolate,
                                Local<Value> val,
                                int64_t* out) {
  if (!val->IsNumber())
    return false;
  // Even though IntegerValue returns int64_t, JavaScript cannot represent
  // the full precision of int64_t, which means some rounding might occur.
  return FromMaybe(val->IntegerValue(isolate->GetCurrentContext()), out);
}

Local<Value> Converter<uint64_t>::ToV8(Isolate* isolate, uint64_t val) {
  return Number::New(isolate, static_cast<double>(val)).As<Value>();
}

bool Converter<uint64_t>::FromV8(Isolate* isolate,
                                 Local<Value> val,
                                 uint64_t* out) {
  if (!val->IsNumber())
    return false;
  return FromMaybe(val->IntegerValue(isolate->GetCurrentContext()), out);
}

Local<Value> Converter<float>::ToV8(Isolate* isolate, float val) {
  return Number::New(isolate, val).As<Value>();
}

bool Converter<float>::FromV8(Isolate* isolate, Local<Value> val, float* out) {
  if (!val->IsNumber())
    return false;
  *out = static_cast<float>(val.As<Number>()->Value());
  return true;
}

Local<Value> Converter<double>::ToV8(Isolate* isolate, double val) {
  return Number::New(isolate, val).As<Value>();
}

bool Converter<double>::FromV8(Isolate* isolate,
                               Local<Value> val,
                               double* out) {
  if (!val->IsNumber())
    return false;
  *out = val.As<Number>()->Value();
  return true;
}

Local<Value> Converter<std::string_view>::ToV8(Isolate* isolate,
                                               const std::string_view& val) {
  return String::NewFromUtf8(isolate, val.data(),
                             v8::NewStringType::kNormal,
                             static_cast<uint32_t>(val.length()))
      .ToLocalChecked();
}

Local<Value> Converter<std::string>::ToV8(Isolate* isolate,
                                          const std::string& val) {
  return Converter<std::string_view>::ToV8(isolate, val);
}

bool Converter<std::string>::FromV8(Isolate* isolate,
                                    Local<Value> val,
                                    std::string* out) {
  if (!val->IsString())
    return false;
  Local<String> str = Local<String>::Cast(val);
  int length = str->Utf8Length(isolate);
  out->resize(length);
  str->WriteUtf8(isolate, &(*out)[0], length, NULL,
                 String::NO_NULL_TERMINATION);
  return true;
}

Local<Value> Converter<std::u16string>::ToV8(Isolate* isolate,
                                             const std::u16string& val) {
  return String::NewFromTwoByte(isolate,
                                reinterpret_cast<const uint16_t*>(val.data()),
                                v8::NewStringType::kNormal, val.size())
      .ToLocalChecked();
}

bool Converter<std::u16string>::FromV8(Isolate* isolate,
                                       Local<Value> val,
                                       std::u16string* out) {
  if (!val->IsString())
    return false;
  Local<String> str = Local<String>::Cast(val);
  int length = str->Length();
  // Note that the reinterpret cast is because on Windows string16 is an alias
  // to wstring, and hence has character type wchar_t not uint16_t.
  str->Write(isolate,
             reinterpret_cast<uint16_t*>(base::WriteInto(out, length + 1)), 0,
             length);
  return true;
}

v8::Local<v8::Value> Converter<base::TimeTicks>::ToV8(v8::Isolate* isolate,
                                                      base::TimeTicks val) {
  return v8::BigInt::New(isolate, val.since_origin().InMicroseconds())
      .As<v8::Value>();
}

Local<Value> Converter<Local<Function>>::ToV8(Isolate* isolate,
                                              Local<Function> val) {
  return val.As<Value>();
}

bool Converter<Local<Function>>::FromV8(Isolate* isolate,
                                        Local<Value> val,
                                        Local<Function>* out) {
  if (!val->IsFunction())
    return false;
  *out = Local<Function>::Cast(val);
  return true;
}

Local<Value> Converter<Local<Object>>::ToV8(Isolate* isolate,
                                            Local<Object> val) {
  return val.As<Value>();
}

bool Converter<Local<Object>>::FromV8(Isolate* isolate,
                                      Local<Value> val,
                                      Local<Object>* out) {
  if (!val->IsObject())
    return false;
  *out = Local<Object>::Cast(val);
  return true;
}

Local<Value> Converter<Local<Promise>>::ToV8(Isolate* isolate,
                                             Local<Promise> val) {
  return val.As<Value>();
}

bool Converter<Local<Promise>>::FromV8(Isolate* isolate,
                                       Local<Value> val,
                                       Local<Promise>* out) {
  if (!val->IsPromise())
    return false;
  *out = Local<Promise>::Cast(val);
  return true;
}

Local<Value> Converter<Local<ArrayBuffer>>::ToV8(Isolate* isolate,
                                                 Local<ArrayBuffer> val) {
  return val.As<Value>();
}

bool Converter<Local<ArrayBuffer>>::FromV8(Isolate* isolate,
                                           Local<Value> val,
                                           Local<ArrayBuffer>* out) {
  if (!val->IsArrayBuffer())
    return false;
  *out = Local<ArrayBuffer>::Cast(val);
  return true;
}

Local<Value> Converter<Local<External>>::ToV8(Isolate* isolate,
                                              Local<External> val) {
  return val.As<Value>();
}

bool Converter<Local<External>>::FromV8(Isolate* isolate,
                                        v8::Local<Value> val,
                                        Local<External>* out) {
  if (!val->IsExternal())
    return false;
  *out = Local<External>::Cast(val);
  return true;
}

Local<Value> Converter<Local<Value>>::ToV8(Isolate* isolate, Local<Value> val) {
  return val;
}

bool Converter<Local<Value>>::FromV8(Isolate* isolate,
                                     Local<Value> val,
                                     Local<Value>* out) {
  *out = val;
  return true;
}

v8::Local<v8::String> StringToSymbol(v8::Isolate* isolate,
                                     const std::string_view& val) {
  return String::NewFromUtf8(isolate, val.data(),
                             v8::NewStringType::kInternalized,
                             static_cast<uint32_t>(val.length()))
      .ToLocalChecked();
}

v8::Local<v8::String> StringToSymbol(v8::Isolate* isolate,
                                     const std::u16string_view& val) {
  return String::NewFromTwoByte(isolate,
                                reinterpret_cast<const uint16_t*>(val.data()),
                                v8::NewStringType::kInternalized, val.length())
      .ToLocalChecked();
}

base::Location V8ToBaseLocation(const v8::SourceLocation& location) {
  return base::Location::Current(location.Function(), location.FileName(),
                                 location.Line());
}

std::string V8ToString(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty())
    return std::string();
  std::string result;
  if (!ConvertFromV8(isolate, value, &result))
    return std::string();
  return result;
}

}  // namespace gin
