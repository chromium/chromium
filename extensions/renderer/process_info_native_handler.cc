// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/process_info_native_handler.h"

#include "base/functional/bind.h"
#include "extensions/common/extension_id.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"

namespace extensions {

ProcessInfoNativeHandler::ProcessInfoNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      extension_id_(context->GetExtensionID()) {}
ProcessInfoNativeHandler::~ProcessInfoNativeHandler() = default;

void ProcessInfoNativeHandler::AddRoutes() {
  auto get_extension_id = [](const ExtensionId& extension_id,
                             const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(gin::StringToV8(args.GetIsolate(), extension_id));
  };

  RouteHandlerFunction("GetExtensionId",
                       base::BindRepeating(get_extension_id, extension_id_));
}

}  // namespace extensions
