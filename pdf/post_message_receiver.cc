// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/post_message_receiver.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace chrome_pdf {

// static
gin::WrapperInfo PostMessageReceiver::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
v8::Local<v8::Object> PostMessageReceiver::Create(
    v8::Isolate* isolate,
    base::WeakPtr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  return gin::CreateHandle(
             isolate, new PostMessageReceiver(isolate, std::move(client),
                                              std::move(client_task_runner)))
      .ToV8()
      .As<v8::Object>();
}

PostMessageReceiver::~PostMessageReceiver() = default;

PostMessageReceiver::PostMessageReceiver(
    v8::Isolate* isolate,
    base::WeakPtr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : isolate_(isolate),
      client_(std::move(client)),
      client_task_runner_(std::move(client_task_runner)) {}

gin::ObjectTemplateBuilder PostMessageReceiver::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PostMessageReceiver>::GetObjectTemplateBuilder(isolate)
      .SetMethod("postMessage", &PostMessageReceiver::PostMessage);
}

const char* PostMessageReceiver::GetTypeName() {
  return "ChromePdfPostMessageReceiver";
}

std::unique_ptr<base::Value> PostMessageReceiver::ConvertMessage(
    v8::Local<v8::Value> message) {
  if (!v8_value_converter_)
    v8_value_converter_ = content::V8ValueConverter::Create();

  return v8_value_converter_->FromV8Value(message,
                                          isolate_->GetCurrentContext());
}

void PostMessageReceiver::PostMessage(v8::Local<v8::Value> message) {
  if (!client_)
    return;

  std::unique_ptr<base::Value> converted_message = ConvertMessage(message);
  if (!converted_message) {
    NOTREACHED() << "The PDF Viewer UI should not be sending messages that "
                    "cannot be converted.";
    return;
  }

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&Client::OnMessage, client_,
                                               std::move(*converted_message)));
}

}  // namespace chrome_pdf
