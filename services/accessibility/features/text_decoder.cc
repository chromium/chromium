// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/accessibility/features/text_decoder.h"

#include <memory>

#include "base/logging.h"
#include "base/types/fixed_array.h"
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

  v8::LocalVector<v8::Value> args = arguments->GetAll();
  // Note: We do not support a TextDecoderOptions parameter.
  CHECK_GT(args.size(), 0u);
  CHECK(args[0]->IsArrayBuffer() || args[0]->IsArrayBufferView());

  v8::Local<v8::ArrayBufferView> view;
  if (args[0]->IsArrayBuffer()) {
    v8::Local<v8::ArrayBuffer> array = args[0].As<v8::ArrayBuffer>();
    view = v8::DataView::New(array, 0, array->ByteLength());
  } else {
    view = args[0].As<v8::ArrayBufferView>();
  }

  base::FixedArray<char> bytes(view->ByteLength() + 1);
  view->CopyContents(bytes.data(), view->ByteLength());
  bytes[view->ByteLength()] = 0;

  arguments->Return(std::string(bytes.data()));
}

TextDecoder::TextDecoder(v8::Local<v8::Context> context)
    : RegisteredWrappable(context) {}

}  // namespace ax
