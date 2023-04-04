// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_V8_BINDINGS_UTILS_H_
#define SERVICES_ACCESSIBILITY_FEATURES_V8_BINDINGS_UTILS_H_

#include "gin/arguments.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace ax {

// Provides static utility functions that can be bound to V8.
class BindingsUtils {
 public:
  BindingsUtils() = delete;
  ~BindingsUtils() = delete;
  BindingsUtils(const BindingsUtils&) = delete;
  BindingsUtils operator=(const BindingsUtils&) = delete;

  // Adds bindings for atpconsole.log/warn/error to the `object_template`.
  static void AddAtpConsoleTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate> object_template);

  // Provides a return value for `new <name>()` on the `object_template`.
  static void AddCallHandlerToTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate>& object_template,
      const std::string& name,
      v8::FunctionCallback callback);

  // Provides the return value for `new TextEncoder()`.
  static void CreateTextEncoderCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  // Provides the return value for `new TextDecoder()`.
  static void CreateTextDecoderCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_V8_BINDINGS_UTILS_H_
