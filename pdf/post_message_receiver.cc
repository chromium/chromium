// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/post_message_receiver.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
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
             isolate, new PostMessageReceiver(std::move(client),
                                              std::move(client_task_runner)))
      .ToV8()
      .As<v8::Object>();
}

PostMessageReceiver::~PostMessageReceiver() = default;

PostMessageReceiver::PostMessageReceiver(
    base::WeakPtr<Client> client,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : client_(std::move(client)),
      client_task_runner_(std::move(client_task_runner)) {}

gin::ObjectTemplateBuilder PostMessageReceiver::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PostMessageReceiver>::GetObjectTemplateBuilder(isolate)
      .SetMethod("postMessage", &PostMessageReceiver::PostMessage);
}

const char* PostMessageReceiver::GetTypeName() {
  return "ChromePdfPostMessageReceiver";
}

void PostMessageReceiver::PostMessage(v8::Local<v8::Value> message) {
  if (!client_)
    return;

  NOTIMPLEMENTED_LOG_ONCE();

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::OnMessage, client_, base::Value()));
}

}  // namespace chrome_pdf
