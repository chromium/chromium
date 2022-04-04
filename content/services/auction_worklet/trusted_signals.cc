// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "content/services/auction_worklet/auction_downloader.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "net/base/escape.h"
#include "net/base/parse_number.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

namespace {

// Creates a query param of the form `&<name>=<values in comma-delimited list>`.
// Returns an empty string if `keys` is empty. `name` will not be escaped, but
// `values` will be. Each entry in `keys` will be added at most once.
std::string CreateQueryParam(const char* name,
                             const std::set<std::string>& keys) {
  if (keys.empty())
    return std::string();

  std::string query_param = base::StringPrintf("&%s=", name);
  bool first_key = true;
  for (const auto& key : keys) {
    if (first_key) {
      first_key = false;
    } else {
      query_param.append(",");
    }
    query_param.append(net::EscapeQueryParamValue(key, /*use_plus=*/true));
  }
  return query_param;
}

GURL SetQueryParam(const GURL& base_url, const std::string& new_query_params) {
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_params);
  return base_url.ReplaceComponents(replacements);
}

// Extracts GURL/JSON key/value pairs from `v8_object`, using values in `keys`
// as keys. Does not add entries to the map for keys with missing values.
std::map<std::string, std::string> ParseKeyValueMap(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_object,
    const std::set<std::string>& keys) {
  std::map<std::string, std::string> out;
  if (keys.empty())
    return out;

  for (const auto& key : keys) {
    v8::Local<v8::String> v8_key;
    if (!v8_helper->CreateUtf8String(key).ToLocal(&v8_key))
      continue;

    v8::Local<v8::Value> v8_value;
    v8::Local<v8::Value> v8_string_value;
    std::string value;
    // Only the Get() call should be able to fail.
    if (!v8_object->Get(v8_helper->scratch_context(), v8_key)
             .ToLocal(&v8_value) ||
        !v8::JSON::Stringify(v8_helper->scratch_context(), v8_value)
             .ToLocal(&v8_string_value) ||
        !gin::ConvertFromV8(v8_helper->isolate(), v8_string_value, &value)) {
      continue;
    }
    out[key] = std::move(value);
  }
  return out;
}

// Extracts GURL/JSON key/value pairs from the object named `name` in
// `v8_object`, using values in `keys` as keys. Does not add entries to the map
// for keys with missing values.
std::map<std::string, std::string> ParseChildKeyValueMap(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_object,
    const char* name,
    const std::set<std::string>& keys) {
  std::map<std::string, std::string> out;
  if (keys.empty())
    return out;

  v8::Local<v8::Value> named_object_value;
  // Don't consider the entire object missing a fatal error.
  if (!v8_object
           ->Get(v8_helper->scratch_context(),
                 v8_helper->CreateStringFromLiteral(name))
           .ToLocal(&named_object_value) ||
      !named_object_value->IsObject()) {
    return out;
  }

  return ParseKeyValueMap(v8_helper, named_object_value.As<v8::Object>(), keys);
}

// Takes a list of keys, a map of strings to JSON strings and creates a
// corresponding v8::Object from the entries with the provided keys. `keys` must
// not be empty.
v8::Local<v8::Object> CreateObjectFromMap(
    const std::vector<std::string>& keys,
    const std::map<std::string, std::string>& json_data,
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!keys.empty());

  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());
  for (const auto& key : keys) {
    auto data = json_data.find(key);
    // InsertJsonValue() shouldn't be able to fail, but the first check might.
    if (data == json_data.end() ||
        !v8_helper->InsertJsonValue(context, key, data->second, out)) {
      bool result =
          v8_helper->InsertValue(key, v8::Null(v8_helper->isolate()), out);
      DCHECK(result);
    }
  }
  return out;
}

}  // namespace

TrustedSignals::Result::Result(
    std::map<std::string, std::string> bidder_json_data,
    absl::optional<uint32_t> data_version)
    : bidder_json_data_(std::move(bidder_json_data)),
      data_version_(data_version) {}

TrustedSignals::Result::Result(
    std::map<std::string, std::string> render_url_json_data,
    std::map<std::string, std::string> ad_component_json_data,
    absl::optional<uint32_t> data_version)
    : render_url_json_data_(std::move(render_url_json_data)),
      ad_component_json_data_(std::move(ad_component_json_data)),
      data_version_(data_version) {}

v8::Local<v8::Object> TrustedSignals::Result::GetBiddingSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const std::vector<std::string>& bidding_signals_keys) const {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(bidder_json_data_.has_value());

  return CreateObjectFromMap(bidding_signals_keys, *bidder_json_data_,
                             v8_helper, context);
}

v8::Local<v8::Object> TrustedSignals::Result::GetScoringSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls) const {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(render_url_json_data_.has_value());
  DCHECK(ad_component_json_data_.has_value());

  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());

  // Create renderUrl sub-object, and add it to to `out`.
  v8::Local<v8::Object> render_url_v8_object =
      CreateObjectFromMap(std::vector<std::string>{render_url.spec()},
                          *render_url_json_data_, v8_helper, context);
  bool result = v8_helper->InsertValue("renderUrl", render_url_v8_object, out);
  DCHECK(result);

  // If there are any ad components, assemble and add an `adComponentRenderUrls`
  // object as well.
  if (!ad_component_render_urls.empty()) {
    v8::Local<v8::Object> ad_components_v8_object = CreateObjectFromMap(
        ad_component_render_urls, *ad_component_json_data_, v8_helper, context);
    bool result = v8_helper->InsertValue("adComponentRenderUrls",
                                         ad_components_v8_object, out);
    DCHECK(result);
  }

  return out;
}

TrustedSignals::Result::~Result() = default;

std::unique_ptr<TrustedSignals> TrustedSignals::LoadBiddingSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::set<std::string> bidding_signals_keys,
    const std::string& hostname,
    const GURL& trusted_bidding_signals_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback) {
  DCHECK(!bidding_signals_keys.empty());

  std::unique_ptr<TrustedSignals> trusted_signals = base::WrapUnique(
      new TrustedSignals(std::move(bidding_signals_keys),
                         /*render_urls=*/absl::nullopt,
                         /*ad_component_render_urls=*/absl::nullopt,
                         trusted_bidding_signals_url, std::move(v8_helper),
                         std::move(load_signals_callback)));

  std::string query_params =
      "hostname=" + net::EscapeQueryParamValue(hostname, /*use_plus=*/true) +
      CreateQueryParam("keys", *trusted_signals->bidding_signals_keys_);
  GURL full_signals_url =
      SetQueryParam(trusted_bidding_signals_url, query_params);
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.TrustedBidding",
      full_signals_url.spec().size());
  trusted_signals->StartDownload(url_loader_factory, full_signals_url);

  return trusted_signals;
}

std::unique_ptr<TrustedSignals> TrustedSignals::LoadScoringSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::set<std::string> render_urls,
    std::set<std::string> ad_component_render_urls,
    const std::string& hostname,
    const GURL& trusted_scoring_signals_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback) {
  DCHECK(!render_urls.empty());

  std::unique_ptr<TrustedSignals> trusted_signals =
      base::WrapUnique(new TrustedSignals(
          /*bidding_signals_keys=*/absl::nullopt, std::move(render_urls),
          std::move(ad_component_render_urls), trusted_scoring_signals_url,
          std::move(v8_helper), std::move(load_signals_callback)));

  std::string query_params =
      "hostname=" + net::EscapeQueryParamValue(hostname, /*use_plus=*/true) +
      CreateQueryParam("renderUrls", *trusted_signals->render_urls_) +
      CreateQueryParam("adComponentRenderUrls",
                       *trusted_signals->ad_component_render_urls_);
  GURL full_signals_url =
      SetQueryParam(trusted_scoring_signals_url, query_params);
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.TrustedScoring",
      full_signals_url.spec().size());
  trusted_signals->StartDownload(url_loader_factory, full_signals_url);

  return trusted_signals;
}

TrustedSignals::TrustedSignals(
    absl::optional<std::set<std::string>> bidding_signals_keys,
    absl::optional<std::set<std::string>> render_urls,
    absl::optional<std::set<std::string>> ad_component_render_urls,
    const GURL& trusted_signals_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback)
    : bidding_signals_keys_(std::move(bidding_signals_keys)),
      render_urls_(std::move(render_urls)),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      trusted_signals_url_(trusted_signals_url),
      v8_helper_(std::move(v8_helper)),
      load_signals_callback_(std::move(load_signals_callback)) {
  DCHECK(v8_helper_);
  DCHECK(load_signals_callback_);

  // Either this should be for bidding signals or scoring signals.
  DCHECK(bidding_signals_keys_ || (render_urls_ && ad_component_render_urls_));
  DCHECK(!bidding_signals_keys_ ||
         (!render_urls_ && !ad_component_render_urls_));
}

TrustedSignals::~TrustedSignals() = default;

void TrustedSignals::StartDownload(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& full_signals_url) {
  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, full_signals_url, AuctionDownloader::MimeType::kJson,
      base::BindOnce(&TrustedSignals::OnDownloadComplete,
                     base::Unretained(this)));
}

void TrustedSignals::OnDownloadComplete(
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    absl::optional<std::string> error_msg) {
  // The downloader's job is done, so clean it up.
  auction_downloader_.reset();

  // Key-related fields aren't needed after this call, so pass ownership of them
  // over to the parser on the V8 thread.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedSignals::HandleDownloadResultOnV8Thread,
                     v8_helper_, trusted_signals_url_,
                     std::move(bidding_signals_keys_), std::move(render_urls_),
                     std::move(ad_component_render_urls_), std::move(body),
                     std::move(headers), std::move(error_msg),
                     base::SequencedTaskRunnerHandle::Get(),
                     weak_ptr_factory.GetWeakPtr()));
}

// static
void TrustedSignals::HandleDownloadResultOnV8Thread(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& signals_url,
    absl::optional<std::set<std::string>> bidding_signals_keys,
    absl::optional<std::set<std::string>> render_urls,
    absl::optional<std::set<std::string>> ad_component_render_urls,
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    absl::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedSignals> weak_instance) {
  if (!body) {
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error_msg));
    return;
  }
  DCHECK(!error_msg.has_value());

  uint32_t data_version;
  std::string data_version_string;
  if (headers &&
      headers->GetNormalizedHeader("Data-Version", &data_version_string) &&
      !net::ParseUint32(data_version_string, &data_version)) {
    std::string error = base::StringPrintf(
        "Rejecting load of %s due to invalid Data-Version header: %s",
        signals_url.spec().c_str(), data_version_string.c_str());
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error));
    return;
  }

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  v8::Local<v8::Value> v8_data;
  if (!v8_helper->CreateValueFromJson(v8_helper->scratch_context(), *body)
           .ToLocal(&v8_data) ||
      !v8_data->IsObject()) {
    std::string error = base::StrCat(
        {signals_url.spec(), " Unable to parse as a JSON object."});
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error));
    return;
  }

  v8::Local<v8::Object> v8_object = v8_data.As<v8::Object>();

  scoped_refptr<Result> result;

  absl::optional<uint32_t> maybe_data_version;
  if (!data_version_string.empty())
    maybe_data_version = data_version;

  if (bidding_signals_keys) {
    // Handle bidding signals case.
    base::UmaHistogramCounts10M(
        "Ads.InterestGroup.Net.ResponseSizeBytes.TrustedBidding", body->size());
    result = base::MakeRefCounted<Result>(
        ParseKeyValueMap(v8_helper.get(), v8_object, *bidding_signals_keys),
        maybe_data_version);
  } else {
    // Handle scoring signals case.
    base::UmaHistogramCounts10M(
        "Ads.InterestGroup.Net.ResponseSizeBytes.TrustedScoring", body->size());
    result = base::MakeRefCounted<Result>(
        ParseChildKeyValueMap(v8_helper.get(), v8_object, "renderUrls",
                              *render_urls),
        ParseChildKeyValueMap(v8_helper.get(), v8_object,
                              "adComponentRenderUrls",
                              *ad_component_render_urls),
        maybe_data_version);
  }

  PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                           std::move(result), absl::nullopt);
}

void TrustedSignals::PostCallbackToUserThread(
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedSignals> weak_instance,
    scoped_refptr<Result> result,
    absl::optional<std::string> error_msg) {
  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedSignals::DeliverCallbackOnUserThread,
                     weak_instance, std::move(result), std::move(error_msg)));
}

void TrustedSignals::DeliverCallbackOnUserThread(
    scoped_refptr<Result> result,
    absl::optional<std::string> error_msg) {
  std::move(load_signals_callback_)
      .Run(std::move(result), std::move(error_msg));
}

}  // namespace auction_worklet
