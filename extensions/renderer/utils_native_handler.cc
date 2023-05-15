// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/utils_native_handler.h"

#include "base/functional/bind.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"

namespace extensions {

UtilsNativeHandler::UtilsNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

UtilsNativeHandler::~UtilsNativeHandler() = default;

void UtilsNativeHandler::AddRoutes() {
  RouteHandlerFunction("deepCopy",
                       base::BindRepeating(&UtilsNativeHandler::DeepCopy,
                                           base::Unretained(this)));
  RouteHandlerFunction(
      "isInServiceWorker",
      base::BindRepeating(&UtilsNativeHandler::IsInServiceWorker,
                          base::Unretained(this)));
}

void UtilsNativeHandler::DeepCopy(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(
      blink::WebSerializedScriptValue::Serialize(isolate, args[0])
          .Deserialize(isolate));
}

void UtilsNativeHandler::IsInServiceWorker(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(0, args.Length());
  const bool is_in_service_worker = context()->IsForServiceWorker();
  args.GetReturnValue().Set(is_in_service_worker);
}

}  // namespace extensions
