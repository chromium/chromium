// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_scoring_signals.h"

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
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

namespace {

// Creates a query param of the form `&<name>=<urls in comma-delimited list>`.
// Returns an empty string if `urls` is empty. `name` will not be escaped, but
// entries in `urls` will be.
std::string CreateQueryParam(const char* name, const std::set<GURL>& urls) {
  if (urls.empty())
    return std::string();
  std::string query_param = base::StringPrintf("&%s=", name);
  bool first_key = true;
  for (const auto& url : urls) {
    if (first_key) {
      first_key = false;
    } else {
      query_param.append(",");
    }
    query_param.append(
        net::EscapeQueryParamValue(url.spec(), /*use_plus=*/true));
  }
  return query_param;
}

// Extracts GURL/JSON key/value pairs from the object named `name` in
// `v8_object`, using values in `urls` as keys. Does not add entries to the map
// for keys with missing values.
std::map<GURL, std::string> ExtractUrlMap(AuctionV8Helper* v8_helper,
                                          v8::Local<v8::Object> v8_object,
                                          const char* name,
                                          const std::set<GURL>& urls) {
  std::map<GURL, std::string> out;
  if (urls.empty())
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

  v8::Local<v8::Object> named_object = named_object_value.As<v8::Object>();
  for (const auto& url : urls) {
    v8::Local<v8::String> v8_key;
    if (!v8_helper->CreateUtf8String(url.spec()).ToLocal(&v8_key))
      continue;

    v8::Local<v8::Value> v8_value;
    v8::Local<v8::Value> v8_string_value;
    std::string value;
    // Only the Get() call should be able to fail.
    if (!named_object->Get(v8_helper->scratch_context(), v8_key)
             .ToLocal(&v8_value) ||
        !v8::JSON::Stringify(v8_helper->scratch_context(), v8_value)
             .ToLocal(&v8_string_value) ||
        !gin::ConvertFromV8(v8_helper->isolate(), v8_string_value, &value)) {
      continue;
    }
    out[url] = std::move(value);
  }
  return out;
}

}  // namespace

TrustedScoringSignals::Result::Result(
    std::map<GURL, std::string> render_url_json_data,
    std::map<GURL, std::string> ad_component_json_data)
    : render_url_json_data_(std::move(render_url_json_data)),
      ad_component_json_data_(std::move(ad_component_json_data)) {}

TrustedScoringSignals::Result::~Result() = default;

v8::Local<v8::Object> TrustedScoringSignals::Result::GetSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const GURL& render_url,
    const std::set<GURL>& ad_component_render_urls) const {
  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());

  // Create renderUrl sub-object, and add it to to `out`.
  v8::Local<v8::Object> render_url_v8_object =
      v8::Object::New(v8_helper->isolate());
  auto render_url_data = render_url_json_data_.find(render_url);
  // InsertJsonValue() shouldn't be able to fail, but the first check might.
  if (render_url_data == render_url_json_data_.end() ||
      !v8_helper->InsertJsonValue(context, render_url.spec(),
                                  render_url_data->second,
                                  render_url_v8_object)) {
    bool result = v8_helper->InsertValue(render_url.spec(),
                                         v8::Null(v8_helper->isolate()),
                                         render_url_v8_object);
    DCHECK(result);
  }
  bool result = v8_helper->InsertValue("renderUrl", render_url_v8_object, out);
  DCHECK(result);

  // If there are any ad components, assemble and add an `adComponentRenderUrls`
  // object as well.
  if (!ad_component_render_urls.empty()) {
    v8::Local<v8::Object> ad_components_v8_object =
        v8::Object::New(v8_helper->isolate());
    for (const auto& url : ad_component_render_urls) {
      auto data = ad_component_json_data_.find(url);
      // InsertJsonValue() shouldn't be able to fail, but the first check might.
      if (data == ad_component_json_data_.end() ||
          !v8_helper->InsertJsonValue(context, url.spec(), data->second,
                                      ad_components_v8_object)) {
        bool result =
            v8_helper->InsertValue(url.spec(), v8::Null(v8_helper->isolate()),
                                   ad_components_v8_object);
        DCHECK(result);
      }
    }
    bool result = v8_helper->InsertValue("adComponentRenderUrls",
                                         ad_components_v8_object, out);
    DCHECK(result);
  }

  return out;
}

TrustedScoringSignals::TrustedScoringSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    std::set<GURL> render_urls,
    std::set<GURL> ad_component_render_urls,
    const std::string& hostname,
    const GURL& trusted_scoring_signals_url,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback)
    : trusted_scoring_signals_url_(trusted_scoring_signals_url),
      v8_helper_(std::move(v8_helper)),
      load_signals_callback_(std::move(load_signals_callback)) {
  DCHECK(v8_helper_);
  // Allow `render_urls` or `ad_component_render_urls` to be empty, but not
  // both.
  DCHECK(!render_urls.empty() || !ad_component_render_urls.empty());
  DCHECK(load_signals_callback_);

  std::string query_params =
      "hostname=" + net::EscapeQueryParamValue(hostname, true) +
      CreateQueryParam("renderUrls", render_urls) +
      CreateQueryParam("adComponentRenderUrls", ad_component_render_urls);

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_params);
  GURL final_url = trusted_scoring_signals_url.ReplaceComponents(replacements);

  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, final_url, AuctionDownloader::MimeType::kJson,
      base::BindOnce(&TrustedScoringSignals::OnDownloadComplete,
                     base::Unretained(this), std::move(render_urls),
                     std::move(ad_component_render_urls)));
}

TrustedScoringSignals::~TrustedScoringSignals() = default;

void TrustedScoringSignals::OnDownloadComplete(
    std::set<GURL> render_urls,
    std::set<GURL> ad_component_render_urls,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg) {
  auction_downloader_.reset();

  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TrustedScoringSignals::HandleDownloadResultOnV8Thread, v8_helper_,
          trusted_scoring_signals_url_, std::move(render_urls),
          std::move(ad_component_render_urls), std::move(body),
          std::move(error_msg), base::SequencedTaskRunnerHandle::Get(),
          weak_ptr_factory.GetWeakPtr()));
}

// static
void TrustedScoringSignals::HandleDownloadResultOnV8Thread(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& trusted_scoring_signals_url,
    std::set<GURL> render_urls,
    std::set<GURL> ad_component_render_urls,
    std::unique_ptr<std::string> body,
    absl::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedScoringSignals> weak_instance) {
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
    std::string error = base::StrCat({trusted_scoring_signals_url.spec(),
                                      " Unable to parse as a JSON object."});
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error));
    return;
  }

  v8::Local<v8::Object> v8_object = v8_data.As<v8::Object>();

  std::map<GURL, std::string> render_url_json_data =
      ExtractUrlMap(v8_helper.get(), v8_object, "renderUrls", render_urls);
  std::map<GURL, std::string> ad_component_json_data =
      ExtractUrlMap(v8_helper.get(), v8_object, "adComponentRenderUrls",
                    ad_component_render_urls);

  PostCallbackToUserThread(
      std::move(user_thread_task_runner), weak_instance,
      std::make_unique<Result>(std::move(render_url_json_data),
                               std::move(ad_component_json_data)),
      absl::nullopt);
}

// static
void TrustedScoringSignals::PostCallbackToUserThread(
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedScoringSignals> weak_instance,
    std::unique_ptr<Result> result,
    absl::optional<std::string> error_msg) {
  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedScoringSignals::DeliverCallbackOnUserThread,
                     weak_instance, std::move(result), std::move(error_msg)));
}

void TrustedScoringSignals::DeliverCallbackOnUserThread(
    std::unique_ptr<Result> result,
    absl::optional<std::string> error_msg) {
  std::move(load_signals_callback_)
      .Run(std::move(result), std::move(error_msg));
}

}  // namespace auction_worklet
