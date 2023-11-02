// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/console.h"

#include <stdio.h>

#include "base/strings/string_util.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "v8/include/v8-template.h"

namespace gin {

namespace {

void Log(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);
  std::vector<std::string> messages;
  if (!args.GetRemaining(&messages)) {
    args.ThrowError();
    return;
  }
  printf("%s\n", base::JoinString(messages, " ").c_str());
}

}  // namespace

// static
void Console::Register(v8::Isolate* isolate,
                       v8::Local<v8::ObjectTemplate> templ) {
  v8::Local<v8::FunctionTemplate> log_templ = v8::FunctionTemplate::New(
      isolate, Log, v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 0,
      v8::ConstructorBehavior::kThrow);

  templ->Set(StringToSymbol(isolate, "log"), log_templ);
}

}  // namespace gin
