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
#include "v8/include/v8-context.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

TrustedBiddingSignals::Result::Result(
    std::map<std::string, std::string> json_data)
    : json_data_(std::move(json_data)) {}

TrustedBiddingSignals::Result::~Result() = default;

v8::Local<v8::Object> TrustedBiddingSignals::Result::GetSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const std::vector<std::string>& trusted_bidding_signals_keys) const {
  v8::Local<v8::Object> v8_object = v8::Object::New(v8_helper->isolate());
  for (const auto& key : trusted_bidding_signals_keys) {
    auto data = json_data_.find(key);
    v8::Local<v8::Value> v8_value;
    // InsertJsonValue() shouldn't be able to fail, but the first check might.
    if (data == json_data_.end() ||
        !v8_helper->InsertJsonValue(context, key, data->second, v8_object)) {
      v8_value = v8::Null(v8_helper->isolate());
      bool result = v8_helper->InsertValue(key, v8::Null(v8_helper->isolate()),
                                           v8_object);
      DCHECK(result);
    }
  }
  return v8_object;
}

TrustedBiddingSignals::TrustedBiddingSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::vector<std::string> trusted_bidding_signals_keys,
    const std::string& hostname,
    const GURL& trusted_bidding_signals_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback)
    : trusted_bidding_signals_url_(trusted_bidding_signals_url),
      v8_helper_(std::move(v8_helper)),
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

void TrustedBiddingSignals::OnDownloadComplete(
    std::vector<std::string> trusted_bidding_signals_keys,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg) {
  auction_downloader_.reset();

  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedBiddingSignals::HandleDownloadResultOnV8Thread,
                     v8_helper_, trusted_bidding_signals_url_,
                     std::move(trusted_bidding_signals_keys), std::move(body),
                     std::move(error_msg),
                     base::SequencedTaskRunnerHandle::Get(),
                     weak_ptr_factory.GetWeakPtr()));
}

// static
void TrustedBiddingSignals::HandleDownloadResultOnV8Thread(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& trusted_bidding_signals_url,
    std::vector<std::string> trusted_bidding_signals_keys,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedBiddingSignals> weak_instance) {
  if (!body) {
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error_msg));
    return;
  }

  DCHECK(!error_msg.has_value());

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::Value> v8_data;
  if (!v8_helper->CreateValueFromJson(v8_helper->scratch_context(), *body)
           .ToLocal(&v8_data) ||
      !v8_data->IsObject()) {
    std::string error = base::StrCat({trusted_bidding_signals_url.spec(),
                                      " Unable to parse as a JSON object."});
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error));
    return;
  }

  v8::Local<v8::Object> v8_object = v8_data.As<v8::Object>();

  std::map<std::string, std::string> json_data;
  for (const auto& key : trusted_bidding_signals_keys) {
    v8::Local<v8::String> v8_key;
    v8::Local<v8::Value> v8_value;
    v8::Local<v8::Value> v8_string_value;
    std::string value;
    if (!v8_helper->CreateUtf8String(key).ToLocal(&v8_key)) {
      PostCallbackToUserThread(std::move(user_thread_task_runner),
                               weak_instance, nullptr, absl::nullopt);

      return;
    }
    // Only the `has_result` check should be able to fail.
    v8::Maybe<bool> has_result =
        v8_object->Has(v8_helper->scratch_context(), v8_key);
    if (has_result.IsNothing() || !has_result.FromJust() ||
        !v8_object->Get(v8_helper->scratch_context(), v8_key)
             .ToLocal(&v8_value) ||
        !v8::JSON::Stringify(v8_helper->scratch_context(), v8_value)
             .ToLocal(&v8_string_value) ||
        !gin::ConvertFromV8(v8_helper->isolate(), v8_string_value, &value)) {
      continue;
    }
    json_data[key] = std::move(value);
  }

  PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                           std::make_unique<Result>(std::move(json_data)),
                           absl::nullopt);
}

// static
void TrustedBiddingSignals::PostCallbackToUserThread(
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedBiddingSignals> weak_instance,
    std::unique_ptr<Result> result,
    absl::optional<std::string> error_msg) {
  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedBiddingSignals::DeliverCallbackOnUserThread,
                     weak_instance, std::move(result), std::move(error_msg)));
}

void TrustedBiddingSignals::DeliverCallbackOnUserThread(
    std::unique_ptr<Result> result,
    absl::optional<std::string> error_msg) {
  std::move(load_signals_callback_)
      .Run(std::move(result), std::move(error_msg));
}

}  // namespace auction_worklet
