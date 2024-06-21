// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_LOGGING_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_LOGGING_NATIVE_HANDLER_H_

#include <string>

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// Exposes logging.h macros to JavaScript bindings.
class LoggingNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit LoggingNativeHandler(ScriptContext* context);
  ~LoggingNativeHandler() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // Equivalent to CHECK(predicate) << message.
  //
  // void(predicate, message?)
  void Check(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Equivalent to DCHECK(predicate) << message.
  //
  // void(predicate, message?)
  void Dcheck(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Equivalent to DCHECK_IS_ON().
  //
  // bool()
  void DcheckIsOn(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Equivalent to LOG(INFO) << message.
  //
  // void(message)
  void Log(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Equivalent to LOG(WARNING) << message.
  //
  // void(message)
  void Warning(const v8::FunctionCallbackInfo<v8::Value>& args);

  void ParseArgs(const v8::FunctionCallbackInfo<v8::Value>& args,
                 bool* check_value,
                 std::string* error_message);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_LOGGING_NATIVE_HANDLER_H_
