// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/console.h"

#include "base/logging.h"
#include "content/services/shared_storage_worklet/worklet_v8_helper.h"
#include "gin/arguments.h"

namespace shared_storage_worklet {

Console::Console(blink::mojom::SharedStorageWorkletServiceClient* client)
    : client_(client) {}

Console::~Console() = default;

gin::WrapperInfo Console::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder Console::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<Console>::GetObjectTemplateBuilder(isolate).SetMethod(
      "log", &Console::Log);
}

const char* Console::GetTypeName() {
  return "Console";
}

void Console::Log(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();

  std::vector<v8::Local<v8::Value>> argument_list = args->GetAll();

  std::string result;
  for (size_t i = 0; i < argument_list.size(); ++i) {
    v8::String::Utf8Value val_utf8(isolate, argument_list[i]);
    if (i != 0)
      result += ' ';
    if (*val_utf8)
      result += std::string(*val_utf8, val_utf8.length());
  }

  client_->ConsoleLog(result);
}

}  // namespace shared_storage_worklet
