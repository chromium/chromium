// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/i18n_custom_bindings.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/renderer/i18n_hooks_util.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

I18NCustomBindings::I18NCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void I18NCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetL10nMessage", "i18n",
      base::Bind(&I18NCustomBindings::GetL10nMessage, base::Unretained(this)));
  RouteHandlerFunction("GetL10nUILanguage", "i18n",
                       base::Bind(&I18NCustomBindings::GetL10nUILanguage,
                                  base::Unretained(this)));
  RouteHandlerFunction("DetectTextLanguage", "i18n",
                       base::Bind(&I18NCustomBindings::DetectTextLanguage,
                                  base::Unretained(this)));
}

void I18NCustomBindings::GetL10nMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString()) {
    NOTREACHED() << "Bad arguments";
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  std::string extension_id;
  if (args[2]->IsNull() || !args[2]->IsString()) {
    return;
  } else {
    extension_id = *v8::String::Utf8Value(isolate, args[2]);
    if (extension_id.empty())
      return;
  }

  std::string message_name = *v8::String::Utf8Value(isolate, args[0]);
  v8::Local<v8::Value> message = i18n_hooks::GetI18nMessage(
      message_name, extension_id, args[1], context()->GetRenderFrame(),
      context()->v8_context());
  args.GetReturnValue().Set(message);
}

void I18NCustomBindings::GetL10nUILanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(),
                              content::RenderThread::Get()->GetLocale().c_str(),
                              v8::NewStringType::kNormal)
          .ToLocalChecked());
}

void I18NCustomBindings::DetectTextLanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsString());

  std::string text = *v8::String::Utf8Value(args.GetIsolate(), args[0]);
  v8::Local<v8::Value> detected_language =
      i18n_hooks::DetectTextLanguage(context()->v8_context(), text);
  args.GetReturnValue().Set(detected_language);
}

}  // namespace extensions
