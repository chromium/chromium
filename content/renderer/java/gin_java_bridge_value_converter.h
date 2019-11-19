// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_VALUE_CONVERTER_H_
#define CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_VALUE_CONVERTER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/renderer/v8_value_converter.h"

namespace content {

class GinJavaBridgeValueConverter : public content::V8ValueConverter::Strategy {
 public:
  CONTENT_EXPORT GinJavaBridgeValueConverter();
  CONTENT_EXPORT ~GinJavaBridgeValueConverter() override;

  CONTENT_EXPORT v8::Local<v8::Value> ToV8Value(
      const base::Value* value,
      v8::Local<v8::Context> context) const;
  CONTENT_EXPORT std::unique_ptr<base::Value> FromV8Value(
      v8::Local<v8::Value> value,
      v8::Local<v8::Context> context) const;

  // content::V8ValueConverter::Strategy
  bool FromV8Object(v8::Local<v8::Object> value,
                    std::unique_ptr<base::Value>* out,
                    v8::Isolate* isolate) override;
  bool FromV8ArrayBuffer(v8::Local<v8::Object> value,
                         std::unique_ptr<base::Value>* out,
                         v8::Isolate* isolate) override;
  bool FromV8Number(v8::Local<v8::Number> value,
                    std::unique_ptr<base::Value>* out) override;
  bool FromV8Undefined(std::unique_ptr<base::Value>* out) override;

 private:
  std::unique_ptr<V8ValueConverter> converter_;

  DISALLOW_COPY_AND_ASSIGN(GinJavaBridgeValueConverter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_JAVA_GIN_JAVA_BRIDGE_VALUE_CONVERTER_H_
