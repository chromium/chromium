// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/console.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "third_party/blink/public/web/web_console_message.h"

namespace extensions {
namespace console {

namespace {

// Writes |message| to stack to show up in minidump, then crashes.
void CheckWithMinidump(const std::string& message) {
  DEBUG_ALIAS_FOR_CSTR(minidump, message.c_str(), 1024);
  CHECK(false) << message;
}

void BoundLogMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  std::string message;
  for (int i = 0; i < info.Length(); ++i) {
    if (i > 0)
      message += " ";
    message += *v8::String::Utf8Value(info.GetIsolate(), info[i]);
  }

  ScriptContext* script_context =
      GetScriptContextFromV8Context(info.GetIsolate()->GetCurrentContext());

  // TODO(devlin): Consider (D)CHECK(script_context)
  const auto level = static_cast<blink::mojom::ConsoleMessageLevel>(
      info.Data().As<v8::Int32>()->Value());
  AddMessage(script_context, level, message);
}

gin::WrapperInfo kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

void Fatal(ScriptContext* context, const std::string& message) {
  AddMessage(context, blink::mojom::ConsoleMessageLevel::kError, message);
  CheckWithMinidump(message);
}

void AddMessage(ScriptContext* script_context,
                blink::mojom::ConsoleMessageLevel level,
                const std::string& message) {
  if (!script_context) {
    LOG(WARNING) << "Could not log \"" << message
                 << "\": no ScriptContext found";
    return;
  }

  if (!script_context->is_valid()) {
    LOG(WARNING) << "Could not log \"" << message
                 << "\": ScriptContext invalidated.";
    return;
  }

  blink::WebConsoleMessage web_console_message(
      level, blink::WebString::FromUTF8(message));
  blink::WebConsoleMessage::LogWebConsoleMessage(script_context->v8_context(),
                                                 web_console_message);
}

v8::Local<v8::Object> AsV8Object(v8::Isolate* isolate) {
  v8::EscapableHandleScope handle_scope(isolate);
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(&kWrapperInfo);
  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New(isolate);
    static const struct {
      const char* name;
      blink::mojom::ConsoleMessageLevel level;
    } methods[] = {
        {"debug", blink::mojom::ConsoleMessageLevel::kVerbose},
        {"log", blink::mojom::ConsoleMessageLevel::kInfo},
        {"warn", blink::mojom::ConsoleMessageLevel::kWarning},
        {"error", blink::mojom::ConsoleMessageLevel::kError},
    };
    for (const auto& method : methods) {
      v8::Local<v8::FunctionTemplate> function = v8::FunctionTemplate::New(
          isolate, BoundLogMethodCallback,
          v8::Integer::New(isolate, static_cast<int>(method.level)));
      function->RemovePrototype();
      templ->Set(gin::StringToSymbol(isolate, method.name), function);
    }
    data->SetObjectTemplate(&kWrapperInfo, templ);
  }
  return handle_scope.Escape(
      templ->NewInstance(isolate->GetCurrentContext()).ToLocalChecked());
}

}  // namespace console
}  // namespace extensions
