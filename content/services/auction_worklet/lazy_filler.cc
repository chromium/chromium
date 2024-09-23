// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/lazy_filler.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

LazyFiller::~LazyFiller() = default;

LazyFiller::LazyFiller(AuctionV8Helper* v8_helper) : v8_helper_(v8_helper) {}

// static
void LazyFiller::SetResult(const v8::PropertyCallbackInfo<v8::Value>& info,
                           v8::Local<v8::Value> result) {
  info.GetReturnValue().Set(result);
}

bool LazyFiller::DefineLazyAttribute(v8::Local<v8::Object> object,
                                     std::string_view name,
                                     v8::AccessorNameGetterCallback getter) {
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Maybe<bool> success = object->SetLazyDataProperty(
      isolate->GetCurrentContext(), gin::StringToSymbol(isolate, name), getter,
      v8::External::New(isolate, this),
      /*attributes=*/v8::None,
      /*getter_side_effect_type=*/v8::SideEffectType::kHasNoSideEffect,
      /*setter_side_effect_type=*/v8::SideEffectType::kHasSideEffect);
  return success.IsJust() && success.FromJust();
}

bool LazyFiller::DefineLazyAttributeWithMetadata(
    v8::Local<v8::Object> object,
    v8::Local<v8::Value> metadata,
    std::string_view name,
    v8::AccessorNameGetterCallback getter,
    v8::Local<v8::ObjectTemplate>& lazy_filler_template) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (lazy_filler_template.IsEmpty()) {
    lazy_filler_template = v8::ObjectTemplate::New(isolate);
    lazy_filler_template->SetInternalFieldCount(2);
  }
  v8::Local<v8::Object> data =
      lazy_filler_template->NewInstance(context).ToLocalChecked();
  data->SetInternalField(0, v8::External::New(isolate, this));
  data->SetInternalField(1, metadata);

  v8::Maybe<bool> success = object->SetLazyDataProperty(
      context, gin::StringToSymbol(isolate, name), getter, data,
      /*attributes=*/v8::None,
      /*getter_side_effect_type=*/v8::SideEffectType::kHasNoSideEffect,
      /*setter_side_effect_type=*/v8::SideEffectType::kHasSideEffect);
  return success.IsJust() && success.FromJust();
}

void* LazyFiller::GetSelfWithMetadataInternal(
    const v8::PropertyCallbackInfo<v8::Value>& info,
    v8::Local<v8::Value>& metadata) {
  v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(info.Data());
  metadata = data->GetInternalField(1).As<v8::Value>();
  return (data->GetInternalField(0).As<v8::Value>().As<v8::External>())
      ->Value();
}

}  // namespace auction_worklet
