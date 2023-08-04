// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/register_ad_beacon_bindings.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace auction_worklet {

RegisterAdBeaconBindings::RegisterAdBeaconBindings(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper) {}

RegisterAdBeaconBindings::~RegisterAdBeaconBindings() = default;

void RegisterAdBeaconBindings::AttachToContext(v8::Local<v8::Context> context) {
  v8::Local<v8::External> v8_this =
      v8::External::New(v8_helper_->isolate(), this);
  v8::Local<v8::Function> v8_function =
      v8::Function::New(context, &RegisterAdBeaconBindings::RegisterAdBeacon,
                        v8_this)
          .ToLocalChecked();
  context->Global()
      ->Set(context, v8_helper_->CreateStringFromLiteral("registerAdBeacon"),
            v8_function)
      .Check();
}

void RegisterAdBeaconBindings::Reset() {
  ad_beacon_map_.clear();
  first_call_ = true;
}

void RegisterAdBeaconBindings::RegisterAdBeacon(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  RegisterAdBeaconBindings* bindings = static_cast<RegisterAdBeaconBindings*>(
      v8::External::Cast(*args.Data())->Value());
  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  if (!bindings->first_call_) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "registerAdBeacon may be called at most once")));
    return;
  }
  bindings->ad_beacon_map_.clear();

  if (args.Length() != 1 || args[0].IsEmpty() || !args[0]->IsObject()) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "registerAdBeacon requires 1 object parameter")));
    return;
  }

  v8::Local<v8::Object> obj = args[0].As<v8::Object>();

  v8::MaybeLocal<v8::Array> maybe_fields = obj->GetOwnPropertyNames(context);
  v8::Local<v8::Array> fields;
  if (!maybe_fields.ToLocal(&fields)) {
    // TODO: This might not be able to happen.
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "registerAdBeacon could not get object attributes")));
    return;
  }
  std::vector<std::pair<std::string, GURL>> ad_beacon_list;
  for (size_t idx = 0; idx < fields->Length(); idx++) {
    v8::Local<v8::Value> key = fields->Get(context, idx).ToLocalChecked();
    if (!key->IsString()) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              "registerAdBeacon object attributes must be strings")));
      return;
    }
    std::string key_string = gin::V8ToString(isolate, key);
    std::string url_string =
        gin::V8ToString(isolate, obj->Get(context, key).ToLocalChecked());
    GURL url(url_string);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      isolate->ThrowException(
          v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
              base::StrCat({"registerAdBeacon invalid reporting url for key '",
                            key_string, "': '", url_string, "'"})
                  .c_str())));
      return;
    }
    ad_beacon_list.emplace_back(key_string, url);
  }
  base::flat_map<std::string, GURL> ad_beacon_map(std::move(ad_beacon_list));
  DCHECK_EQ(fields->Length(), ad_beacon_map.size());

  bindings->first_call_ = false;
  bindings->ad_beacon_map_ = std::move(ad_beacon_map);
}

}  // namespace auction_worklet
