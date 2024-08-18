// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
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
  if (keys.empty()) {
    return std::string();
  }

  std::string query_param = base::StringPrintf("&%s=", name);
  bool first_key = true;
  for (const auto& key : keys) {
    if (first_key) {
      first_key = false;
    } else {
      query_param.append(",");
    }
    query_param.append(base::EscapeQueryParamValue(key, /*use_plus=*/true));
  }
  return query_param;
}

GURL SetQueryParam(const GURL& base_url, const std::string& new_query_params) {
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_params);
  return base_url.ReplaceComponents(replacements);
}

// Extracts key/value pairs from `v8_object`, using values in `keys` as keys.
// Does not add entries to the map for keys with missing values.
std::map<std::string, AuctionV8Helper::SerializedValue> ParseKeyValueMap(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_object,
    const std::set<std::string>& keys) {
  std::map<std::string, AuctionV8Helper::SerializedValue> out;
  if (keys.empty()) {
    return out;
  }

  for (const auto& key : keys) {
    v8::Local<v8::String> v8_key;
    if (!v8_helper->CreateUtf8String(key).ToLocal(&v8_key)) {
      continue;
    }

    // Skip over missing properties (rather than serializing 'undefined') and
    // also things in the prototype.
    v8::Maybe<bool> has_key =
        v8_object->HasOwnProperty(v8_helper->scratch_context(), v8_key);
    if (has_key.IsNothing() || !has_key.FromJust()) {
      continue;
    }

    v8::Local<v8::Value> v8_value;
    if (!v8_object->Get(v8_helper->scratch_context(), v8_key)
             .ToLocal(&v8_value)) {
      continue;
    }
    AuctionV8Helper::SerializedValue serialized_value =
        v8_helper->Serialize(v8_helper->scratch_context(), v8_value);
    if (!serialized_value.IsOK()) {
      continue;
    }
    out[key] = std::move(serialized_value);
  }
  return out;
}

// Extracts key/value pairs from the object named `name` in
// `v8_object`, using values in `keys` as keys. Does not add entries to the map
// for keys with missing values.
std::map<std::string, AuctionV8Helper::SerializedValue> ParseChildKeyValueMap(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_object,
    const char* name,
    const std::set<std::string>& keys) {
  std::map<std::string, AuctionV8Helper::SerializedValue> out;
  if (keys.empty()) {
    return out;
  }

  v8::Local<v8::Value> named_object_value;
  // Don't consider the entire object missing (or values other than objects) a
  // fatal error.
  if (!v8_object
           ->Get(v8_helper->scratch_context(),
                 v8_helper->CreateStringFromLiteral(name))
           .ToLocal(&named_object_value) ||
      !named_object_value->IsObject() ||
      // Arrays are considered objects by Javascript, but they're not the object
      // type we're looking for.
      named_object_value->IsArray()) {
    return out;
  }

  return ParseKeyValueMap(v8_helper, named_object_value.As<v8::Object>(), keys);
}

// Attempts to parse the `perInterestGroupData` value in `v8_object`, extracting
// the `priorityVector` fields of all interest group in `interest_group_names`,
// along with `updateIfOlderThanMs`, and putting them all in the returned
// PerInterestGroupDataMap.
TrustedSignals::Result::PerInterestGroupDataMap ParsePerInterestGroupMap(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_object,
    const std::set<std::string>& interest_group_names) {
  v8::Local<v8::Value> per_group_data_value;
  if (!v8_object
           ->Get(v8_helper->scratch_context(),
                 v8_helper->CreateStringFromLiteral("perInterestGroupData"))
           .ToLocal(&per_group_data_value) ||
      !per_group_data_value->IsObject() ||
      // Arrays are considered objects by Javascript, but they're not the object
      // type we're looking for.
      per_group_data_value->IsArray()) {
    return {};
  }
  v8::Local<v8::Object> per_group_data_object =
      per_group_data_value.As<v8::Object>();

  TrustedSignals::Result::PerInterestGroupDataMap out;
  for (const auto& interest_group_name : interest_group_names) {
    v8::Local<v8::String> v8_name;
    if (!v8_helper->CreateUtf8String(interest_group_name).ToLocal(&v8_name)) {
      continue;
    }

    v8::Local<v8::Value> per_interest_group_data_value;
    if (!per_group_data_object->Get(v8_helper->scratch_context(), v8_name)
             .ToLocal(&per_interest_group_data_value) ||
        !per_interest_group_data_value->IsObject() ||
        // Arrays are considered objects by Javascript, but they're not the
        // object type we're looking for.
        per_group_data_value->IsArray()) {
      continue;
    }

    v8::Local<v8::Object> v8_per_interest_group_data =
        per_interest_group_data_value.As<v8::Object>();
    std::optional<TrustedSignals::Result::PriorityVector> priority_vector =
        TrustedSignals::ParsePriorityVector(v8_helper,
                                            v8_per_interest_group_data);
    std::optional<base::TimeDelta> update_if_older_than;

    if (base::FeatureList::IsEnabled(
            features::kInterestGroupUpdateIfOlderThan)) {
      update_if_older_than = TrustedSignals::ParseUpdateIfOlderThan(
          v8_helper, v8_per_interest_group_data);
    }
    if (priority_vector || update_if_older_than) {
      out.emplace(interest_group_name, TrustedSignals::Result::PerGroupData(
                                           std::move(priority_vector),
                                           std::move(update_if_older_than)));
    }
  }
  return out;
}

// Takes a list of keys, a map of strings to serialized values and creates a
// corresponding v8::Object from the entries with the provided keys. `keys` must
// not be empty.
v8::Local<v8::Object> CreateObjectFromMap(
    const std::vector<std::string>& keys,
    const std::map<std::string, AuctionV8Helper::SerializedValue>&
        serialized_data,
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(!keys.empty());

  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());

  for (const auto& key : keys) {
    auto data = serialized_data.find(key);
    v8::Local<v8::Value> v8_data;
    // Deserialize() shouldn't normally fail, but the first check might.
    if (data == serialized_data.end() ||
        !v8_helper->Deserialize(context, data->second).ToLocal(&v8_data)) {
      v8_data = v8::Null(v8_helper->isolate());
    }
    bool result = v8_helper->InsertValue(key, v8_data, out);
    DCHECK(result);
  }
  return out;
}

}  // namespace

TrustedSignals::Result::PerGroupData::PerGroupData(
    std::optional<PriorityVector> priority_vector,
    std::optional<base::TimeDelta> update_if_older_than)
    : priority_vector(std::move(priority_vector)),
      update_if_older_than(std::move(update_if_older_than)) {}

TrustedSignals::Result::PerGroupData::~PerGroupData() = default;

TrustedSignals::Result::PerGroupData::PerGroupData(PerGroupData&&) = default;
TrustedSignals::Result::PerGroupData&
TrustedSignals::Result::PerGroupData::operator=(PerGroupData&&) = default;

TrustedSignals::Result::Result(
    PerInterestGroupDataMap per_interest_group_data,
    std::map<std::string, AuctionV8Helper::SerializedValue> bidder_data,
    std::optional<uint32_t> data_version)
    : per_interest_group_data_(std::move(per_interest_group_data)),
      bidder_data_(std::move(bidder_data)),
      data_version_(data_version) {}

TrustedSignals::Result::Result(
    std::map<std::string, AuctionV8Helper::SerializedValue> render_url_data,
    std::map<std::string, AuctionV8Helper::SerializedValue> ad_component_data,
    std::optional<uint32_t> data_version)
    : render_url_data_(std::move(render_url_data)),
      ad_component_data_(std::move(ad_component_data)),
      data_version_(data_version) {}

const TrustedSignals::Result::PerGroupData*
TrustedSignals::Result::GetPerGroupData(
    const std::string& interest_group_name) const {
  DCHECK(per_interest_group_data_.has_value());
  auto result = per_interest_group_data_->find(interest_group_name);
  if (result == per_interest_group_data_->end()) {
    return nullptr;
  }
  return &result->second;
}

v8::Local<v8::Object> TrustedSignals::Result::GetBiddingSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const std::vector<std::string>& bidding_signals_keys) const {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(bidder_data_.has_value());

  return CreateObjectFromMap(bidding_signals_keys, *bidder_data_, v8_helper,
                             context);
}

v8::Local<v8::Object> TrustedSignals::Result::GetScoringSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const GURL& render_url,
    const std::vector<std::string>& ad_component_render_urls) const {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  DCHECK(render_url_data_.has_value());
  DCHECK(ad_component_data_.has_value());

  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());

  // Create renderURL sub-object, and add it to `out`.
  v8::Local<v8::Object> render_url_v8_object =
      CreateObjectFromMap(std::vector<std::string>{render_url.spec()},
                          *render_url_data_, v8_helper, context);
  bool result = v8_helper->InsertValue("renderURL", render_url_v8_object, out);
  DCHECK(result);
  // TODO(crbug.com/40266734): Remove deprecated `renderUrl` alias.
  result = v8_helper->InsertValue("renderUrl", render_url_v8_object, out);
  DCHECK(result);

  // If there are any ad components, assemble and add an `adComponentRenderURLs`
  // object as well.
  if (!ad_component_render_urls.empty()) {
    v8::Local<v8::Object> ad_components_v8_object = CreateObjectFromMap(
        ad_component_render_urls, *ad_component_data_, v8_helper, context);
    result = v8_helper->InsertValue("adComponentRenderURLs",
                                    ad_components_v8_object, out);
    // TODO(crbug.com/40266734): Remove deprecated `adComponentRenderUrls`
    // alias.
    result = v8_helper->InsertValue("adComponentRenderUrls",
                                    ad_components_v8_object, out);
    DCHECK(result);
  }

  return out;
}

// static
v8::Local<v8::Value> TrustedSignals::Result::WrapCrossOriginSignals(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    const url::Origin& source_origin,
    v8::Local<v8::Value> signals) {
  v8::Isolate* isolate = v8_helper->isolate();
  if (signals->IsNullOrUndefined()) {
    return v8::Null(isolate);
  }
  v8::Local<v8::Object> out = v8::Object::New(v8_helper->isolate());
  gin::Dictionary out_converter(isolate, out);
  out_converter.Set(source_origin.Serialize(), signals);
  return out;
}

TrustedSignals::Result::~Result() = default;

GURL TrustedSignals::BuildTrustedBiddingSignalsURL(
    const std::string& hostname,
    const GURL& trusted_bidding_signals_url,
    const std::set<std::string>& interest_group_names,
    const std::set<std::string>& bidding_signals_keys,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param) {
  std::string query_params = base::StrCat(
      {"hostname=", base::EscapeQueryParamValue(hostname, /*use_plus=*/true),
       CreateQueryParam("keys", bidding_signals_keys),
       CreateQueryParam("interestGroupNames", interest_group_names)});

  if (experiment_group_id.has_value()) {
    base::StrAppend(&query_params,
                    {"&experimentGroupId=",
                     base::NumberToString(experiment_group_id.value())});
  }
  if (!trusted_bidding_signals_slot_size_param.empty()) {
    base::StrAppend(&query_params,
                    {"&", trusted_bidding_signals_slot_size_param});
  }
  GURL full_signals_url =
      SetQueryParam(trusted_bidding_signals_url, query_params);

  return full_signals_url;
}

GURL TrustedSignals::BuildTrustedScoringSignalsURL(
    const std::string& hostname,
    const GURL& trusted_scoring_signals_url,
    const std::set<std::string>& render_urls,
    const std::set<std::string>& ad_component_render_urls,
    std::optional<uint16_t> experiment_group_id) {
  // TODO(crbug.com/40264073): Find a way to rename renderUrls to renderURLs.
  std::string query_params = base::StrCat(
      {"hostname=", base::EscapeQueryParamValue(hostname, /*use_plus=*/true),
       CreateQueryParam("renderUrls", render_urls),
       CreateQueryParam("adComponentRenderUrls", ad_component_render_urls)});
  if (experiment_group_id.has_value()) {
    base::StrAppend(&query_params,
                    {"&experimentGroupId=",
                     base::NumberToString(experiment_group_id.value())});
  }
  GURL full_signals_url =
      SetQueryParam(trusted_scoring_signals_url, query_params);

  return full_signals_url;
}

std::unique_ptr<TrustedSignals> TrustedSignals::LoadBiddingSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        devtools_pending_remote,
    std::set<std::string> interest_group_names,
    std::set<std::string> bidding_signals_keys,
    const std::string& hostname,
    const GURL& trusted_bidding_signals_url,
    std::optional<uint16_t> experiment_group_id,
    const std::string& trusted_bidding_signals_slot_size_param,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback) {
  DCHECK(!interest_group_names.empty());

  GURL full_signals_url = TrustedSignals::BuildTrustedBiddingSignalsURL(
      hostname, trusted_bidding_signals_url, interest_group_names,
      bidding_signals_keys, experiment_group_id,
      trusted_bidding_signals_slot_size_param);

  std::unique_ptr<TrustedSignals> trusted_signals =
      base::WrapUnique(new TrustedSignals(
          std::move(interest_group_names), std::move(bidding_signals_keys),
          /*render_urls=*/std::nullopt,
          /*ad_component_render_urls=*/std::nullopt,
          trusted_bidding_signals_url, std::move(devtools_pending_remote),
          std::move(v8_helper), std::move(load_signals_callback)));

  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.TrustedBidding",
      full_signals_url.spec().size());
  trusted_signals->StartDownload(url_loader_factory, full_signals_url);

  return trusted_signals;
}

std::unique_ptr<TrustedSignals> TrustedSignals::LoadScoringSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    std::set<std::string> render_urls,
    std::set<std::string> ad_component_render_urls,
    const std::string& hostname,
    const GURL& trusted_scoring_signals_url,
    std::optional<uint16_t> experiment_group_id,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback) {
  DCHECK(!render_urls.empty());

  GURL full_signals_url = BuildTrustedScoringSignalsURL(
      hostname, trusted_scoring_signals_url, render_urls,
      ad_component_render_urls, experiment_group_id);

  std::unique_ptr<TrustedSignals> trusted_signals =
      base::WrapUnique(new TrustedSignals(
          /*interest_group_names=*/std::nullopt,
          /*bidding_signals_keys=*/std::nullopt, std::move(render_urls),
          std::move(ad_component_render_urls), trusted_scoring_signals_url,
          std::move(auction_network_events_handler), std::move(v8_helper),
          std::move(load_signals_callback)));

  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.RequestUrlSizeBytes.TrustedScoring",
      full_signals_url.spec().size());
  trusted_signals->StartDownload(url_loader_factory, full_signals_url);

  return trusted_signals;
}

std::optional<TrustedSignals::Result::PriorityVector>
TrustedSignals::ParsePriorityVector(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_per_interest_group_data) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  v8::Local<v8::Value> priority_vector_value;
  if (!v8_per_interest_group_data
           ->Get(v8_helper->scratch_context(),
                 v8_helper->CreateStringFromLiteral("priorityVector"))
           .ToLocal(&priority_vector_value) ||
      !priority_vector_value->IsObject() ||
      // Arrays are considered objects, so check explicitly for them.
      priority_vector_value->IsArray()) {
    return std::nullopt;
  }

  v8::Local<v8::Object> priority_vector_object =
      priority_vector_value.As<v8::Object>();

  v8::Local<v8::Array> priority_vector_keys;
  if (!priority_vector_object->GetOwnPropertyNames(v8_helper->scratch_context())
           .ToLocal(&priority_vector_keys)) {
    return std::nullopt;
  }

  // Put together a list to construct the returned `flat_map` with, since
  // insertion is O(n) while construction is O(n log n).
  std::vector<std::pair<std::string, double>> priority_vector_pairs;
  for (uint32_t i = 0; i < priority_vector_keys->Length(); ++i) {
    v8::Local<v8::Value> v8_key;
    std::string key;
    if (!priority_vector_keys->Get(v8_helper->scratch_context(), i)
             .ToLocal(&v8_key) ||
        !v8_key->IsString() ||
        !gin::ConvertFromV8(v8_helper->isolate(), v8_key, &key)) {
      continue;
    }
    v8::Local<v8::Value> v8_value;
    double value;
    if (!priority_vector_object->Get(v8_helper->scratch_context(), v8_key)
             .ToLocal(&v8_value) ||
        !v8_value->IsNumber() ||
        !v8_value->NumberValue(v8_helper->scratch_context()).To(&value)) {
      continue;
    }
    priority_vector_pairs.emplace_back(std::move(key), value);
  }
  return TrustedSignals::Result::PriorityVector(
      std::move(priority_vector_pairs));
}

std::optional<base::TimeDelta> TrustedSignals::ParseUpdateIfOlderThan(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Object> v8_per_interest_group_data) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  v8::Local<v8::Value> update_if_older_than_ms_value;
  double update_if_older_than_ms;
  if (!v8_per_interest_group_data
           ->Get(v8_helper->scratch_context(),
                 v8_helper->CreateStringFromLiteral("updateIfOlderThanMs"))
           .ToLocal(&update_if_older_than_ms_value) ||
      !update_if_older_than_ms_value->IsNumber() ||
      !update_if_older_than_ms_value->NumberValue(v8_helper->scratch_context())
           .To(&update_if_older_than_ms)) {
    return std::nullopt;
  }
  return base::Milliseconds(update_if_older_than_ms);
}

TrustedSignals::TrustedSignals(
    std::optional<std::set<std::string>> interest_group_names,
    std::optional<std::set<std::string>> bidding_signals_keys,
    std::optional<std::set<std::string>> render_urls,
    std::optional<std::set<std::string>> ad_component_render_urls,
    const GURL& trusted_signals_url,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadSignalsCallback load_signals_callback)
    : interest_group_names_(std::move(interest_group_names)),
      bidding_signals_keys_(std::move(bidding_signals_keys)),
      render_urls_(std::move(render_urls)),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      trusted_signals_url_(trusted_signals_url),
      v8_helper_(std::move(v8_helper)),
      load_signals_callback_(std::move(load_signals_callback)),
      auction_network_events_handler_(
          std::move(auction_network_events_handler)) {
  DCHECK(v8_helper_);
  DCHECK(load_signals_callback_);

  // Either this should be for bidding signals or scoring signals.
  DCHECK((interest_group_names_ && bidding_signals_keys_) ||
         (render_urls_ && ad_component_render_urls_));
  DCHECK((!interest_group_names_ && !bidding_signals_keys_) ||
         (!render_urls_ && !ad_component_render_urls_));
}

TrustedSignals::~TrustedSignals() = default;

void TrustedSignals::StartDownload(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& full_signals_url) {
  download_start_time_ = base::TimeTicks::Now();

  std::unique_ptr<MojoNetworkEventsDelegate> network_events_delegate;

  if (auction_network_events_handler_.is_valid()) {
    network_events_delegate = std::make_unique<MojoNetworkEventsDelegate>(
        std::move(auction_network_events_handler_));
  }
  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, full_signals_url,
      AuctionDownloader::DownloadMode::kActualDownload,
      AuctionDownloader::MimeType::kJson,
      /*post_body=*/std::nullopt, /*content_type=*/std::nullopt,
      AuctionDownloader::ResponseStartedCallback(),
      base::BindOnce(&TrustedSignals::OnDownloadComplete,
                     base::Unretained(this)),
      /*network_events_delegate=*/std::move(network_events_delegate));
}

void TrustedSignals::OnDownloadComplete(
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error_msg) {
  // The downloader's job is done, so clean it up.
  auction_downloader_.reset();

  // Key-related fields aren't needed after this call, so pass ownership of them
  // over to the parser on the V8 thread.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedSignals::HandleDownloadResultOnV8Thread,
                     v8_helper_, trusted_signals_url_,
                     std::move(interest_group_names_),
                     std::move(bidding_signals_keys_), std::move(render_urls_),
                     std::move(ad_component_render_urls_), std::move(body),
                     std::move(headers), std::move(error_msg),
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     weak_ptr_factory.GetWeakPtr(),
                     base::TimeTicks::Now() - download_start_time_));
}

// static
void TrustedSignals::HandleDownloadResultOnV8Thread(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& signals_url,
    std::optional<std::set<std::string>> interest_group_names,
    std::optional<std::set<std::string>> bidding_signals_keys,
    std::optional<std::set<std::string>> render_urls,
    std::optional<std::set<std::string>> ad_component_render_urls,
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedSignals> weak_instance,
    base::TimeDelta download_time) {
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
      !net::ParseUint32(data_version_string,
                        net::ParseIntFormat::STRICT_NON_NEGATIVE,
                        &data_version)) {
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
      !v8_data->IsObject() ||
      // v8 considers arrays a subtype of object, but the response body must be
      // a JSON object, not a JSON array, so need to explicitly check if it's an
      // array.
      v8_data->IsArray()) {
    std::string error = base::StrCat(
        {signals_url.spec(), " Unable to parse as a JSON object."});
    PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                             nullptr, std::move(error));
    return;
  }

  v8::Local<v8::Object> v8_object = v8_data.As<v8::Object>();

  scoped_refptr<Result> result;

  std::optional<uint32_t> maybe_data_version;
  if (!data_version_string.empty()) {
    maybe_data_version = data_version;
  }

  if (bidding_signals_keys) {
    // Handle bidding signals case.
    base::UmaHistogramCounts10M(
        "Ads.InterestGroup.Net.ResponseSizeBytes.TrustedBidding", body->size());
    base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.TrustedBidding",
                            download_time);
    int format_version = 1;
    std::string format_version_string;
    if (headers &&
        (headers->GetNormalizedHeader(
             "Ad-Auction-Bidding-Signals-Format-Version",
             &format_version_string) ||
         headers->GetNormalizedHeader("X-fledge-bidding-signals-format-version",
                                      &format_version_string))) {
      if (!base::StringToInt(format_version_string, &format_version) ||
          (format_version != 1 && format_version != 2)) {
        std::string error = base::StringPrintf(
            "Rejecting load of %s due to unrecognized Format-Version header: "
            "%s",
            signals_url.spec().c_str(), format_version_string.c_str());
        PostCallbackToUserThread(std::move(user_thread_task_runner),
                                 weak_instance, nullptr, std::move(error));
        return;
      }
    }
    base::UmaHistogramBoolean(
        "Ads.InterestGroup.ReceivedDeprecatedBiddingSignalsFormat",
        format_version != 2);
    if (format_version == 1) {
      result = base::MakeRefCounted<Result>(
          /*per_interest_group_data=*/TrustedSignals::Result::
              PerInterestGroupDataMap(),
          ParseKeyValueMap(v8_helper.get(), v8_object, *bidding_signals_keys),
          maybe_data_version);
      error_msg = base::StringPrintf(
          "Bidding signals URL %s is using outdated bidding signals format. "
          "Consumers should be updated to use bidding signals format version 2",
          signals_url.spec().c_str());
    } else {
      DCHECK_EQ(format_version, 2);
      result = base::MakeRefCounted<Result>(
          ParsePerInterestGroupMap(v8_helper.get(), v8_object,
                                   *interest_group_names),
          ParseChildKeyValueMap(v8_helper.get(), v8_object, "keys",
                                *bidding_signals_keys),
          maybe_data_version);
    }
  } else {
    // Handle scoring signals case.
    base::UmaHistogramCounts10M(
        "Ads.InterestGroup.Net.ResponseSizeBytes.TrustedScoring", body->size());
    base::UmaHistogramTimes("Ads.InterestGroup.Net.DownloadTime.TrustedScoring",
                            download_time);

    // TODO(crbug.com/40266734): Remove deprecated `renderUrl` alias.
    auto render_urls_map = ParseChildKeyValueMap(v8_helper.get(), v8_object,
                                                 "renderURLs", *render_urls);
    auto render_urls_map_deprecated = ParseChildKeyValueMap(
        v8_helper.get(), v8_object, "renderUrls", *render_urls);
    auto ad_component_render_urls_map = ParseChildKeyValueMap(
        v8_helper.get(), v8_object, "adComponentRenderURLs",
        *ad_component_render_urls);
    auto ad_component_render_urls_map_deprecated = ParseChildKeyValueMap(
        v8_helper.get(), v8_object, "adComponentRenderUrls",
        *ad_component_render_urls);
    result = base::MakeRefCounted<Result>(
        !render_urls_map.empty() ? std::move(render_urls_map)
                                 : std::move(render_urls_map_deprecated),
        !ad_component_render_urls_map.empty()
            ? std::move(ad_component_render_urls_map)
            : std::move(ad_component_render_urls_map_deprecated),
        maybe_data_version);
  }

  PostCallbackToUserThread(std::move(user_thread_task_runner), weak_instance,
                           std::move(result), std::move(error_msg));
}

void TrustedSignals::PostCallbackToUserThread(
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedSignals> weak_instance,
    scoped_refptr<Result> result,
    std::optional<std::string> error_msg) {
  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedSignals::DeliverCallbackOnUserThread,
                     weak_instance, std::move(result), std::move(error_msg)));
}

void TrustedSignals::DeliverCallbackOnUserThread(
    scoped_refptr<Result> result,
    std::optional<std::string> error_msg) {
  std::move(load_signals_callback_)
      .Run(std::move(result), std::move(error_msg));
}

}  // namespace auction_worklet
