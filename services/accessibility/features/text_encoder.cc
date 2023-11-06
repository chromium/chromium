// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/text_encoder.h"

#include <cstring>
#include <memory>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "services/accessibility/features/registered_wrappable.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-typed-array.h"

namespace ax {

// static
gin::WrapperInfo TextEncoder::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
gin::Handle<TextEncoder> TextEncoder::Create(v8::Local<v8::Context> context) {
  return gin::CreateHandle(context->GetIsolate(), new TextEncoder(context));
}

gin::ObjectTemplateBuilder TextEncoder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  // Note: We do not support TextEncoder::encodeInto.
  return gin::Wrappable<TextEncoder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("encode", &TextEncoder::Encode);
}

void TextEncoder::Encode(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::LocalVector<v8::Value> args = arguments->GetAll();
  CHECK_GT(args.size(), 0u);
  CHECK(args[0]->IsString());
  v8::Local<v8::String> v8_input = args[0].As<v8::String>();
  std::string input;
  gin::ConvertFromV8(isolate, v8_input, &input);

  int num_bytes = input.size();
  void* buffer =
      gin::ArrayBufferAllocator::SharedInstance()->Allocate(num_bytes);
  auto deleter = [](void* buffer, size_t length, void* data) {
    gin::ArrayBufferAllocator::SharedInstance()->Free(buffer, length);
  };
  std::unique_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(buffer, num_bytes, deleter, nullptr);

  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, std::move(backing_store));
  if (num_bytes) {
    CHECK(array_buffer->Data());
    memcpy(array_buffer->Data(), input.c_str(), num_bytes);
  }
  v8::Local<v8::Uint8Array> result =
      v8::Uint8Array::New(array_buffer, 0, num_bytes);
  arguments->GetFunctionCallbackInfo()->GetReturnValue().Set(result);
}

TextEncoder::TextEncoder(v8::Local<v8::Context> context)
    : RegisteredWrappable(context) {}

}  // namespace ax
