// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/set_icon_natives.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/common_param_traits.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/script_context.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const char kInvalidDimensions[] = "ImageData has invalid dimensions.";
const char kInvalidData[] = "ImageData data length does not match dimensions.";
const char kNoMemory[] = "Chrome was unable to initialize icon.";

}  // namespace

namespace extensions {

SetIconNatives::SetIconNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void SetIconNatives::AddRoutes() {
  RouteHandlerFunction(
      "SetIconCommon",
      base::Bind(&SetIconNatives::SetIconCommon, base::Unretained(this)));
}

bool SetIconNatives::ConvertImageDataToBitmapValue(
    const v8::Local<v8::Object> image_data,
    v8::Local<v8::Value>* image_data_bitmap) {
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::Local<v8::Object> data =
      image_data
          ->Get(v8::String::NewFromUtf8(isolate, "data",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          ->ToObject(isolate);
  int width = image_data
                  ->Get(v8::String::NewFromUtf8(
                            isolate, "width", v8::NewStringType::kInternalized)
                            .ToLocalChecked())
                  ->Int32Value(v8_context)
                  .FromMaybe(0);
  int height =
      image_data
          ->Get(v8::String::NewFromUtf8(isolate, "height",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          ->Int32Value(v8_context)
          .FromMaybe(0);

  if (width <= 0 || height <= 0) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidDimensions,
                                v8::NewStringType::kInternalized)
            .ToLocalChecked()));
    return false;
  }

  // We need to be able to safely check |data_length| == 4 * width * height
  // without overflowing below.
  int max_width = (std::numeric_limits<int>::max() / 4) / height;
  if (width > max_width) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidDimensions,
                                v8::NewStringType::kInternalized)
            .ToLocalChecked()));
    return false;
  }

  int data_length =
      data->Get(v8::String::NewFromUtf8(isolate, "length",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          ->Int32Value(v8_context)
          .FromMaybe(0);
  if (data_length != 4 * width * height) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidData,
                                v8::NewStringType::kInternalized)
            .ToLocalChecked()));
    return false;
  }

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(width, height)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kNoMemory,
                                v8::NewStringType::kInternalized)
            .ToLocalChecked()));
    return false;
  }
  bitmap.eraseARGB(0, 0, 0, 0);

  uint32_t* pixels = bitmap.getAddr32(0, 0);
  for (int t = 0; t < width * height; t++) {
    // |data| is RGBA, pixels is ARGB.
    pixels[t] =
        SkPreMultiplyColor(((data->Get(v8::Integer::New(isolate, 4 * t + 3))
                                 ->Int32Value(v8_context)
                                 .FromMaybe(0) &
                             0xFF)
                            << 24) |
                           ((data->Get(v8::Integer::New(isolate, 4 * t + 0))
                                 ->Int32Value(v8_context)
                                 .FromMaybe(0) &
                             0xFF)
                            << 16) |
                           ((data->Get(v8::Integer::New(isolate, 4 * t + 1))
                                 ->Int32Value(v8_context)
                                 .FromMaybe(0) &
                             0xFF)
                            << 8) |
                           ((data->Get(v8::Integer::New(isolate, 4 * t + 2))
                                 ->Int32Value(v8_context)
                                 .FromMaybe(0) &
                             0xFF)
                            << 0));
  }

  // Construct the Value object.
  IPC::Message bitmap_pickle;
  IPC::WriteParam(&bitmap_pickle, bitmap);
  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::Create(bitmap_pickle.size(), 1);
  memcpy(buffer.Data(), bitmap_pickle.data(), bitmap_pickle.size());
  *image_data_bitmap = blink::WebArrayBufferConverter::ToV8Value(
      &buffer, context()->v8_context()->Global(), isolate);

  return true;
}

bool SetIconNatives::ConvertImageDataSetToBitmapValueSet(
    v8::Local<v8::Object>& details,
    v8::Local<v8::Object>* bitmap_set_value) {
  v8::Isolate* isolate = context()->v8_context()->GetIsolate();
  v8::Local<v8::Object> image_data_set =
      details
          ->Get(v8::String::NewFromUtf8(isolate, "imageData",
                                        v8::NewStringType::kInternalized)
                    .ToLocalChecked())
          ->ToObject(isolate);

  DCHECK(bitmap_set_value);

  v8::Local<v8::Array> property_names(image_data_set->GetOwnPropertyNames());
  for (size_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> key(property_names->Get(i));
    v8::String::Utf8Value utf8_key(isolate, key);
    int size;
    if (!base::StringToInt(std::string(*utf8_key), &size))
      continue;
    v8::Local<v8::Object> image_data =
        image_data_set->Get(key)->ToObject(isolate);
    v8::Local<v8::Value> image_data_bitmap;
    if (!ConvertImageDataToBitmapValue(image_data, &image_data_bitmap))
      return false;
    (*bitmap_set_value)->Set(key, image_data_bitmap);
  }
  return true;
}

void SetIconNatives::SetIconCommon(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsObject());
  v8::Local<v8::Object> details = args[0]->ToObject(args.GetIsolate());
  v8::Local<v8::Object> bitmap_set_value(v8::Object::New(args.GetIsolate()));
  if (!ConvertImageDataSetToBitmapValueSet(details, &bitmap_set_value))
    return;

  v8::Local<v8::Object> dict(v8::Object::New(args.GetIsolate()));
  dict->Set(v8::String::NewFromUtf8(args.GetIsolate(), "imageData",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            bitmap_set_value);
  v8::Local<v8::String> tabId =
      v8::String::NewFromUtf8(args.GetIsolate(), "tabId",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  bool has_tabid = false;
  if (details->Has(context()->v8_context(), tabId).To(&has_tabid) &&
      has_tabid) {
    dict->Set(tabId, details->Get(tabId));
  }
  args.GetReturnValue().Set(dict);
}

}  // namespace extensions
