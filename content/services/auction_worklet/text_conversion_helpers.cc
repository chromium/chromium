// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/text_conversion_helpers.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_worklet_features.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-typed-array.h"

namespace auction_worklet {

TextConversionHelpers::TextConversionHelpers(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

void TextConversionHelpers::AttachToContext(v8::Local<v8::Context> context) {
  if (!base::FeatureList::IsEnabled(features::kFledgeTextConversionHelpers)) {
    return;
  }

  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> encode =
      v8::Function::New(context, &TextConversionHelpers::EncodeUtf8, v8_this)
          .ToLocalChecked();
  v8::Local<v8::Function> decode =
      v8::Function::New(context, &TextConversionHelpers::DecodeUtf8, v8_this)
          .ToLocalChecked();
  v8::Local<v8::Object> pa_object = v8::Object::New(v8_helper_->isolate());
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("protectedAudience"),
            pa_object)
      .Check();
  pa_object
      ->Set(context, v8_helper_->CreateStringFromLiteral("encodeUtf8"), encode)
      .Check();
  pa_object
      ->Set(context, v8_helper_->CreateStringFromLiteral("decodeUtf8"), decode)
      .Check();
}

void TextConversionHelpers::Reset() {}

void TextConversionHelpers::EncodeUtf8(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  TextConversionHelpers* bindings = static_cast<TextConversionHelpers*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope, "encodeUtf8 ",
                               &args,
                               /*min_required_args=*/1);
  if (!args_converter.is_success()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  // We don't use webidl_compat to do the string conversion here since we don't
  // want the output as a std::string, we want to stick it into a UInt8Array.
  // (Also it would get DOMString and not USVString semantics, but that's
  // secondary).
  v8::Local<v8::String> v8_string;
  IdlConvert::Status conversion_status;
  {
    if (args[0]->IsString()) {
      v8_string = args[0].As<v8::String>();
    } else {
      v8::TryCatch try_catch(isolate);
      if (!args[0]
               ->ToString(isolate->GetCurrentContext())
               .ToLocal(&v8_string)) {
        conversion_status = IdlConvert::MakeConversionFailure(
            try_catch, "encodeUtf8 ", {"argument 0"}, "String");
      }
    }
  }

  if (!conversion_status.is_success()) {
    conversion_status.PropagateErrorsToV8(v8_helper);
    return;
  }

  size_t required_len = v8_string->Utf8LengthV2(isolate);
  v8::Local<v8::ArrayBuffer> buffer;
  if (!v8::ArrayBuffer::MaybeNew(
           isolate, required_len,
           v8::BackingStoreInitializationMode::kZeroInitialized)
           .ToLocal(&buffer)) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "Unable to allocate buffer for result")));
    return;
  }

  size_t used_len = v8_string->WriteUtf8V2(
      isolate, reinterpret_cast<char*>(buffer->Data()), buffer->ByteLength(),
      v8::String::WriteFlags::kReplaceInvalidUtf8);

  // The length should be as expected (despite Utf8LengthV2 not knowing about
  // kReplaceInvalidUtf8) since a mismatched surrogate and the unicode
  // replacement character both end up the same length (3 bytes).
  DCHECK_EQ(used_len, required_len);
  args.GetReturnValue().Set(v8::Uint8Array::New(buffer,
                                                /*byte_offset=*/0,
                                                /*length=*/used_len));
}

void TextConversionHelpers::DecodeUtf8(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  TextConversionHelpers* bindings = static_cast<TextConversionHelpers*>(
      v8::External::Cast(*args.Data())->Value());
  AuctionV8Helper* v8_helper = bindings->v8_helper_;
  v8::Isolate* isolate = v8_helper->isolate();

  if (!args[0]->IsUint8Array()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "decodeUtf8 expects a Uint8Array argument")));
    return;
  }

  v8::Local<v8::Uint8Array> array = args[0].As<v8::Uint8Array>();
  // SAFETY: Uint8Array data is from ByteOffset and of length ByteLength inside
  // the ArrayBuffer it wraps.
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(
          isolate,
          UNSAFE_BUFFERS(reinterpret_cast<char*>(array->Buffer()->Data()) +
                         array->ByteOffset()),
          v8::NewStringType::kNormal, array->ByteLength())
          .ToLocalChecked());
}

}  // namespace auction_worklet
