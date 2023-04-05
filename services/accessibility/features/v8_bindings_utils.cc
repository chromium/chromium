// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_bindings_utils.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gin/arguments.h"
#include "services/accessibility/features/text_decoder.h"
#include "services/accessibility/features/text_encoder.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace ax {

namespace {

// Below are for debugging until we can see console.log/warn/error output.
// TODO(crbug.com/1355633): Use blink::mojom::DevToolsAgent interface to attach
// to Chrome devtools.
static std::string PrintArgs(gin::Arguments* args) {
  std::string statement;
  while (!args->PeekNext().IsEmpty()) {
    v8::String::Utf8Value value(args->isolate(), args->PeekNext());
    statement += base::StringPrintf("%s ", *value);
    args->Skip();
  }
  return statement;
}

// Provides temporary functionality for atpconsole.log.
static void ConsoleLog(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Info: " << PrintArgs(args);
}

// Provides temporary functionality for atpconsole.warn.
static void ConsoleWarn(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Error: " << PrintArgs(args);
}

// Provides temporary functionality for atpconsole.error.
static void ConsoleError(gin::Arguments* args) {
  LOG(ERROR) << "AccessibilityService V8: Error: " << PrintArgs(args);
}

}  // namespace

// static
void BindingsUtils::AddAtpConsoleTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template) {
  // Use static bindings for console functions for initial development.
  // Note that "console" seems to be protected in v8 so we have to make
  // our own, "atpconsole".
  // TODO(crbug.com/1355633): Use blink::mojom::DevToolsAgent interface to
  // attach to Chrome devtools and remove these temporary bindings.
  v8::Local<v8::ObjectTemplate> console_template =
      v8::ObjectTemplate::New(isolate);
  console_template->Set(
      isolate, "log",
      gin::CreateFunctionTemplate(isolate, base::BindRepeating(&ConsoleLog)));
  console_template->Set(
      isolate, "warn",
      gin::CreateFunctionTemplate(isolate, base::BindRepeating(&ConsoleWarn)));
  console_template->Set(
      isolate, "error",
      gin::CreateFunctionTemplate(isolate, base::BindRepeating(&ConsoleError)));
  object_template->Set(isolate, "atpconsole", console_template);
}

// static
void BindingsUtils::AddCallHandlerToTemplate(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate>& object_template,
    const std::string& name,
    v8::FunctionCallback callback) {
  v8::Local<v8::FunctionTemplate> fn_template =
      v8::FunctionTemplate::New(isolate);
  fn_template->SetCallHandler(callback);
  object_template->Set(isolate, name.c_str(), fn_template);
}

// static
void BindingsUtils::CreateTextEncoderCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // Check this is a constructor call: JS should always request
  // `new TextEncoder()` rather than just `TextEncoder`.
  DCHECK(info.IsConstructCall());
  gin::Handle<TextEncoder> text_encoder =
      TextEncoder::Create(info.GetIsolate()->GetCurrentContext());
  info.GetReturnValue().Set(text_encoder.ToV8());
}

// static
void BindingsUtils::CreateTextDecoderCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  // Check this is a constructor call: JS should always request
  // `new TextDecoder()` rather than just `TextDecoder`.
  DCHECK(info.IsConstructCall());
  gin::Handle<TextDecoder> text_decoder =
      TextDecoder::Create(info.GetIsolate()->GetCurrentContext());
  info.GetReturnValue().Set(text_decoder.ToV8());
}

}  // namespace ax
