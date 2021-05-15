// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_bidding_signals.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "net/base/escape.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace auction_worklet {

TrustedBiddingSignals::TrustedBiddingSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::vector<std::string> trusted_bidding_signals_keys,
    const std::string& hostname,
    const GURL& trusted_bidding_signals_url,
    AuctionV8Helper* v8_helper,
    LoadSignalsCallback load_signals_callback)
    : trusted_bidding_signals_url_(trusted_bidding_signals_url),
      v8_helper_(v8_helper),
      load_signals_callback_(std::move(load_signals_callback)) {
  DCHECK(!trusted_bidding_signals_keys.empty());
  DCHECK(load_signals_callback_);

  std::string query_params =
      "hostname=" + net::EscapeQueryParamValue(hostname, true);

  query_params += "&keys=";
  bool first_key = true;
  for (const auto& key : trusted_bidding_signals_keys) {
    if (first_key) {
      first_key = false;
    } else {
      query_params.append(",");
    }
    query_params.append(net::EscapeQueryParamValue(key, true));
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_params);
  GURL final_url = trusted_bidding_signals_url.ReplaceComponents(replacements);

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, final_url, AuctionDownloader::MimeType::kJson,
      base::BindOnce(&TrustedBiddingSignals::OnDownloadComplete,
                     base::Unretained(this),
                     std::move(trusted_bidding_signals_keys)));
}

TrustedBiddingSignals::~TrustedBiddingSignals() = default;

v8::Local<v8::Object> TrustedBiddingSignals::GetSignals(
    v8::Local<v8::Context> context,
    const std::vector<std::string>& trusted_bidding_signals_keys) const {
  v8::Local<v8::Object> v8_object = v8::Object::New(v8_helper_->isolate());
  for (const auto& key : trusted_bidding_signals_keys) {
    auto data = json_data_.find(key);
    v8::Local<v8::Value> v8_value;
    // InsertJsonValue() shouldn't be able to fail, but the first check might.
    if (data == json_data_.end() ||
        !v8_helper_->InsertJsonValue(context, key, data->second, v8_object)) {
      v8_value = v8::Null(v8_helper_->isolate());
      bool result = v8_helper_->InsertValue(
          key, v8::Null(v8_helper_->isolate()), v8_object);
      DCHECK(result);
    }
  }
  return v8_object;
}

void TrustedBiddingSignals::OnDownloadComplete(
    std::vector<std::string> trusted_bidding_signals_keys,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg) {
  auction_downloader_.reset();

  if (!body) {
    std::move(load_signals_callback_).Run(false, std::move(error_msg));
    return;
  }

  DCHECK(!error_msg.has_value());

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_);
  v8::Context::Scope context_scope(v8_helper_->scratch_context());

  v8::Local<v8::Value> v8_data;
  if (!v8_helper_->CreateValueFromJson(v8_helper_->scratch_context(), *body)
           .ToLocal(&v8_data) ||
      !v8_data->IsObject()) {
    std::string error = base::StrCat({trusted_bidding_signals_url_.spec(),
                                      " Unable to parse as a JSON object."});
    std::move(load_signals_callback_).Run(false, std::move(error));
    return;
  }

  v8::Local<v8::Object> v8_object = v8_data.As<v8::Object>();

  for (const auto& key : trusted_bidding_signals_keys) {
    v8::Local<v8::String> v8_key;
    v8::Local<v8::Value> v8_value;
    v8::Local<v8::Value> v8_string_value;
    std::string value;
    if (!v8_helper_->CreateUtf8String(key).ToLocal(&v8_key)) {
      std::move(load_signals_callback_).Run(false, absl::nullopt);
      return;
    }
    // Only the `has_result` check should be able to fail.
    v8::Maybe<bool> has_result =
        v8_object->Has(v8_helper_->scratch_context(), v8_key);
    if (has_result.IsNothing() || !has_result.FromJust() ||
        !v8_object->Get(v8_helper_->scratch_context(), v8_key)
             .ToLocal(&v8_value) ||
        !v8::JSON::Stringify(v8_helper_->scratch_context(), v8_value)
             .ToLocal(&v8_string_value) ||
        !gin::ConvertFromV8(v8_helper_->isolate(), v8_string_value, &value)) {
      continue;
    }
    json_data_[key] = std::move(value);
  }
  std::move(load_signals_callback_).Run(true, absl::nullopt);
}

}  // namespace auction_worklet
