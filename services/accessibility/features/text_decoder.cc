// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/text_decoder.h"

#include <memory>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"

namespace ax {

// static
gin::WrapperInfo TextDecoder::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
gin::Handle<TextDecoder> TextDecoder::Create(v8::Local<v8::Context> context) {
  return gin::CreateHandle(context->GetIsolate(), new TextDecoder(context));
}

gin::ObjectTemplateBuilder TextDecoder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<TextDecoder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("decode", &TextDecoder::Decode);
}

// See third_party/blink/renderer/modules/encoding/text_decoder.h
void TextDecoder::Decode(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  std::vector<v8::Local<v8::Value>> args = arguments->GetAll();
  // Note: We do not support a TextDecoderOptions parameter.
  CHECK_GT(args.size(), 0u);
  CHECK(args[0]->IsArrayBuffer() || args[0]->IsArrayBufferView());
  void* bytes = nullptr;
  size_t num_bytes = 0;
  if (args[0]->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> array = args[0].As<v8::ArrayBuffer>();
    bytes = array->Data();
    num_bytes = array->ByteLength();
  } else {
    v8::Local<v8::ArrayBufferView> view = args[0].As<v8::ArrayBufferView>();
    num_bytes = view->ByteLength();
    void* bites[num_bytes];
    view->CopyContents(bites, num_bytes);
    bytes = bites;
  }
  char result[num_bytes + 1];
  for (size_t i = 0; i < num_bytes; i++) {
    result[i] = *(static_cast<char*>(bytes) + i);
  }
  result[num_bytes] = 0;

  arguments->Return(std::string(result));
}

TextDecoder::TextDecoder(v8::Local<v8::Context> context)
    : RegisteredWrappable(context) {}

}  // namespace ax
