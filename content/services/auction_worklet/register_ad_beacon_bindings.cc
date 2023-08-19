// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/register_ad_beacon_bindings.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/webidl_compat.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"

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
  AuctionV8Helper* v8_helper = bindings->v8_helper_;

  if (!bindings->first_call_) {
    isolate->ThrowException(
        v8::Exception::TypeError(v8_helper->CreateStringFromLiteral(
            "registerAdBeacon may be called at most once")));
    return;
  }
  bindings->ad_beacon_map_.clear();

  AuctionV8Helper::TimeLimitScope time_limit_scope(v8_helper->GetTimeLimit());
  ArgsConverter args_converter(v8_helper, time_limit_scope,
                               "registerAdBeacon(): ", &args,
                               /*min_required_args=*/1);
  std::vector<std::pair<std::string, std::string>> idl_map;
  if (args_converter.is_success()) {
    args_converter.SetStatus(ConvertRecord(
        v8_helper, time_limit_scope, "registerAdBeacon(): ", {"argument 'map'"},
        args[0], idl_map));
  }

  if (args_converter.is_failed()) {
    args_converter.TakeStatus().PropagateErrorsToV8(v8_helper);
    return;
  }

  std::vector<std::pair<std::string, GURL>> ad_beacon_list;
  for (const auto& [key_string, url_string] : idl_map) {
    GURL url(url_string);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      std::string error_msg =
          base::StrCat({"registerAdBeacon(): invalid reporting url for key '",
                        key_string, "': '", url_string, "'"});
      // Since the map key is a DOMString and not USVString, the above may
      // not be valid UTF8, so we may need to simplify the error message.
      if (!base::IsStringUTF8(error_msg)) {
        error_msg = "registerAdBeacon(): invalid reporting url";
      }
      isolate->ThrowException(v8::Exception::TypeError(
          v8_helper->CreateUtf8String(error_msg).ToLocalChecked()));
      return;
    }
    ad_beacon_list.emplace_back(std::move(key_string), std::move(url));
  }
  base::flat_map<std::string, GURL> ad_beacon_map(std::move(ad_beacon_list));

  bindings->first_call_ = false;
  bindings->ad_beacon_map_ = std::move(ad_beacon_map);
}

}  // namespace auction_worklet
