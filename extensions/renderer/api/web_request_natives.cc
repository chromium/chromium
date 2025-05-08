// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/web_request_natives.h"

#include <string>

#include "base/functional/bind.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

WebRequestNatives::WebRequestNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void WebRequestNatives::AddRoutes() {
  RouteHandlerFunction(
      "AllowAsyncResponsesForAllEvents",
      base::BindRepeating(&WebRequestNatives::AllowAsyncResponsesForAllEvents,
                          base::Unretained(this)));
}

void WebRequestNatives::AllowAsyncResponsesForAllEvents(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(0, args.Length());

  const Extension* extension = context()->extension();
  bool always_allowed_async_handlers =
      extension && extension->manifest_version() >= 3 &&
      Manifest::IsPolicyLocation(extension->location());

  args.GetReturnValue().Set(always_allowed_async_handlers);
}

}  // namespace extensions
