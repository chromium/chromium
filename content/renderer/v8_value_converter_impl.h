// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
#define CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/renderer/v8_value_converter.h"

namespace content {

class CONTENT_EXPORT V8ValueConverterImpl : public V8ValueConverter {
 public:
  V8ValueConverterImpl();

  V8ValueConverterImpl(const V8ValueConverterImpl&) = delete;
  V8ValueConverterImpl& operator=(const V8ValueConverterImpl&) = delete;

  // V8ValueConverter implementation.
  void SetDateAllowed(bool val) override;
  void SetRegExpAllowed(bool val) override;
  void SetFunctionAllowed(bool val) override;
  void SetStripNullFromObjects(bool val) override;
  void SetConvertNegativeZeroToInt(bool val) override;
  void SetStrategy(Strategy* strategy) override;
  v8::Local<v8::Value> ToV8Value(base::ValueView value,
                                 v8::Local<v8::Context> context) override;
  std::unique_ptr<base::Value> FromV8Value(
      v8::Local<v8::Value> value,
      v8::Local<v8::Context> context) override;

 private:
  friend class ScopedAvoidIdentityHashForTesting;

  class FromV8ValueState;
  class ScopedUniquenessGuard;

  v8::Local<v8::Value> ToV8ValueImpl(v8::Isolate* isolate,
                                     v8::Local<v8::Object> creation_context,
                                     base::ValueView value) const;
  v8::Local<v8::Value> ToV8Array(v8::Isolate* isolate,
                                 v8::Local<v8::Object> creation_context,
                                 const base::Value::List& list) const;
  v8::Local<v8::Value> ToV8Object(v8::Isolate* isolate,
                                  v8::Local<v8::Object> creation_context,
                                  const base::Value::Dict& dictionary) const;
  v8::Local<v8::Value> ToArrayBuffer(
      v8::Isolate* isolate,
      v8::Local<v8::Object> creation_context,
      const base::Value::BlobStorage& value) const;

  std::unique_ptr<base::Value> FromV8ValueImpl(FromV8ValueState* state,
                                               v8::Local<v8::Value> value,
                                               v8::Isolate* isolate) const;
  std::unique_ptr<base::Value> FromV8Array(v8::Local<v8::Array> array,
                                           FromV8ValueState* state,
                                           v8::Isolate* isolate) const;

  // This will convert objects of type ArrayBuffer or any of the
  // ArrayBufferView subclasses.
  std::unique_ptr<base::Value> FromV8ArrayBuffer(v8::Local<v8::Object> val,
                                                 v8::Isolate* isolate) const;

  std::unique_ptr<base::Value> FromV8Object(v8::Local<v8::Object> object,
                                            FromV8ValueState* state,
                                            v8::Isolate* isolate) const;

  // If true, we will convert Date JavaScript objects to doubles.
  bool date_allowed_;

  // If true, we will convert RegExp JavaScript objects to string.
  bool reg_exp_allowed_;

  // If true, we will convert Function JavaScript objects to dictionaries.
  bool function_allowed_;

  // If true, undefined and null values are ignored when converting v8 objects
  // into Values.
  bool strip_null_from_objects_;

  // If true, convert -0 to an integer value (instead of a double).
  bool convert_negative_zero_to_int_;

  bool avoid_identity_hash_for_testing_;

  // Strategy object that changes the converter's behavior.
  raw_ptr<Strategy, DanglingUntriaged> strategy_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_V8_VALUE_CONVERTER_IMPL_H_
