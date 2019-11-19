// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/set_icon_natives.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/common_param_traits.h"
#include "extensions/renderer/script_context.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/blink/public/web/web_array_buffer_converter.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const char kInvalidDimensions[] = "ImageData has invalid dimensions.";
const char kInvalidData[] = "ImageData data length does not match dimensions.";
const char kNoMemory[] = "Chrome was unable to initialize icon.";

void ThrowException(v8::Isolate* isolate, const char* error_message) {
  isolate->ThrowException(v8::Exception::Error(
      v8::String::NewFromUtf8(isolate, error_message,
                              v8::NewStringType::kInternalized)
          .ToLocalChecked()));
}

int GetIntPropertyFromV8Object(v8::Local<v8::Object> v8_object,
                               v8::Local<v8::Context> v8_context,
                               const char* property_name) {
  v8::Local<v8::Value> v8_property_value;
  if (!v8_object
           ->Get(v8_context, v8::String::NewFromUtf8(
                                 v8_context->GetIsolate(), property_name,
                                 v8::NewStringType::kInternalized)
                                 .ToLocalChecked())
           .ToLocal(&v8_property_value)) {
    return 0;
  }
  return v8_property_value->Int32Value(v8_context).FromMaybe(0);
}

int GetIntPropertyFromV8Object(v8::Local<v8::Object> v8_object,
                               v8::Local<v8::Context> v8_context,
                               int index) {
  v8::Local<v8::Value> v8_property_value;
  if (!v8_object
           ->Get(v8_context, v8::Integer::New(v8_context->GetIsolate(), index))
           .ToLocal(&v8_property_value)) {
    return 0;
  }
  return v8_property_value->Int32Value(v8_context).FromMaybe(0);
}

}  // namespace

namespace extensions {

SetIconNatives::SetIconNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void SetIconNatives::AddRoutes() {
  RouteHandlerFunction("SetIconCommon",
                       base::BindRepeating(&SetIconNatives::SetIconCommon,
                                           base::Unretained(this)));
}

bool SetIconNatives::ConvertImageDataToBitmapValue(
    const v8::Local<v8::Object> image_data,
    v8::Local<v8::Value>* image_data_bitmap) {
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::Local<v8::Value> value;
  if (!image_data
           ->Get(v8_context,
                 v8::String::NewFromUtf8(isolate, "data",
                                         v8::NewStringType::kInternalized)
                     .ToLocalChecked())
           .ToLocal(&value)) {
    ThrowException(isolate, kInvalidData);
    return false;
  }

  v8::Local<v8::Object> data;
  if (!value->ToObject(v8_context).ToLocal(&data)) {
    ThrowException(isolate, kInvalidData);
    return false;
  }

  int width = GetIntPropertyFromV8Object(image_data, v8_context, "width");
  int height = GetIntPropertyFromV8Object(image_data, v8_context, "height");
  if (width <= 0 || height <= 0) {
    ThrowException(isolate, kInvalidDimensions);
    return false;
  }

  // We need to be able to safely check |data_length| == 4 * width * height
  // without overflowing below.
  int max_width = (std::numeric_limits<int>::max() / 4) / height;
  if (width > max_width) {
    ThrowException(isolate, kInvalidDimensions);
    return false;
  }

  int data_length = GetIntPropertyFromV8Object(data, v8_context, "length");
  if (data_length != 4 * width * height) {
    ThrowException(isolate, kInvalidData);
    return false;
  }

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(width, height)) {
    ThrowException(isolate, kNoMemory);
    return false;
  }
  bitmap.eraseARGB(0, 0, 0, 0);

  uint32_t* pixels = bitmap.getAddr32(0, 0);
  for (int t = 0; t < width * height; t++) {
    // |data| is RGBA, pixels is ARGB.
    pixels[t] = SkPreMultiplyColor(
        ((GetIntPropertyFromV8Object(data, v8_context, 4 * t + 3) & 0xFF)
         << 24) |
        ((GetIntPropertyFromV8Object(data, v8_context, 4 * t + 0) & 0xFF)
         << 16) |
        ((GetIntPropertyFromV8Object(data, v8_context, 4 * t + 1) & 0xFF)
         << 8) |
        ((GetIntPropertyFromV8Object(data, v8_context, 4 * t + 2) & 0xFF)
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
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::Local<v8::Value> v8_value;
  if (!details
           ->Get(v8_context,
                 v8::String::NewFromUtf8(isolate, "imageData",
                                         v8::NewStringType::kInternalized)
                     .ToLocalChecked())
           .ToLocal(&v8_value)) {
    return false;
  }
  v8::Local<v8::Object> image_data_set;
  if (!v8_value->ToObject(v8_context).ToLocal(&image_data_set)) {
    return false;
  }

  DCHECK(bitmap_set_value);

  v8::Local<v8::Array> property_names(
      image_data_set->GetOwnPropertyNames(v8_context)
          .FromMaybe(v8::Local<v8::Array>()));
  for (size_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> key =
        property_names->Get(v8_context, i).ToLocalChecked();
    v8::String::Utf8Value utf8_key(isolate, key);
    int size;
    if (!base::StringToInt(std::string(*utf8_key), &size))
      continue;
    v8::Local<v8::Value> v8_image_value;
    if (!image_data_set->Get(v8_context, key).ToLocal(&v8_image_value)) {
      return false;
    }
    v8::Local<v8::Object> image_data;
    if (!v8_image_value->ToObject(v8_context).ToLocal(&image_data)) {
      return false;
    }
    v8::Local<v8::Value> image_data_bitmap;
    if (!ConvertImageDataToBitmapValue(image_data, &image_data_bitmap))
      return false;
    (*bitmap_set_value)
        ->Set(v8_context, key, image_data_bitmap)
        .FromMaybe(false);
  }
  return true;
}

void SetIconNatives::SetIconCommon(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsObject());
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Local<v8::Object> details = args[0].As<v8::Object>();
  v8::Local<v8::Object> bitmap_set_value(v8::Object::New(args.GetIsolate()));
  if (!ConvertImageDataSetToBitmapValueSet(details, &bitmap_set_value))
    return;

  v8::Local<v8::Object> dict(v8::Object::New(args.GetIsolate()));
  dict->Set(v8_context,
            v8::String::NewFromUtf8(args.GetIsolate(), "imageData",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked(),
            bitmap_set_value)
      .FromMaybe(false);
  v8::Local<v8::String> tabId =
      v8::String::NewFromUtf8(args.GetIsolate(), "tabId",
                              v8::NewStringType::kInternalized)
          .ToLocalChecked();
  bool has_tabid = false;
  if (details->Has(v8_context, tabId).To(&has_tabid) && has_tabid) {
    dict->Set(v8_context, tabId,
              details->Get(v8_context, tabId).ToLocalChecked())
        .FromMaybe(false);
  }
  args.GetReturnValue().Set(dict);
}

}  // namespace extensions
