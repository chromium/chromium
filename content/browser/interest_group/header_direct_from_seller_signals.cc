// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/header_direct_from_seller_signals.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

HeaderDirectFromSellerSignals::Result::Result() = default;

HeaderDirectFromSellerSignals::Result::Result(
    absl::optional<std::string> seller_signals,
    absl::optional<std::string> auction_signals,
    base::flat_map<url::Origin, std::string> per_buyer_signals)
    : seller_signals_(std::move(seller_signals)),
      auction_signals_(std::move(auction_signals)),
      per_buyer_signals_(std::move(per_buyer_signals)) {}

HeaderDirectFromSellerSignals::Result::~Result() = default;

HeaderDirectFromSellerSignals::HeaderDirectFromSellerSignals() = default;

HeaderDirectFromSellerSignals::~HeaderDirectFromSellerSignals() = default;

// TODO(crbug.com/1462720): Add UMA for response size and for when processing
// doesn't complete by destruction.
void HeaderDirectFromSellerSignals::ParseAndFind(
    const url::Origin& origin,
    const std::string& ad_slot,
    ParseAndFindCompletedCallback callback) {
  ParseAndFindCompletedInfo completed_info{
      /*start_time=*/base::TimeTicks::Now(), /*origin=*/std::move(origin),
      /*ad_slot=*/std::move(ad_slot), /*callback=*/std::move(callback)};
  if (!add_witness_for_origin_completed_callback_) {
    ParseAndFindCompleted(std::move(completed_info));
    return;
  }

  // NOTE: If signals are received faster than the queue can process them, then
  // `callback` will never be called. However, this seems unlikely, given that
  // there should be only a small number of Ad-Auction-Signals header fetches
  // per page. Note that each HeaderDirectFromSellerSignals is per-page.
  parse_and_find_completed_infos_.push(std::move(completed_info));
}

void HeaderDirectFromSellerSignals::AddWitnessForOrigin(
    data_decoder::DataDecoder& decoder,
    const url::Origin& origin,
    const std::string& response,
    AddWitnessForOriginCompletedCallback callback) {
  CHECK(callback);
  unprocessed_header_responses_.emplace(origin, response);
  if (!add_witness_for_origin_completed_callback_) {
    add_witness_for_origin_completed_callback_ = std::move(callback);
    DecodeNextResponse(decoder,
                       /*errors=*/std::vector<std::string>());
  }
}

HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo::
    ParseAndFindCompletedInfo(base::TimeTicks start_time,
                              url::Origin origin,
                              std::string ad_slot,
                              ParseAndFindCompletedCallback callback)
    : origin(std::move(origin)),
      ad_slot(std::move(ad_slot)),
      callback(std::move(callback)) {}

HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo::
    ~ParseAndFindCompletedInfo() = default;

HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo::
    ParseAndFindCompletedInfo(ParseAndFindCompletedInfo&&) = default;

HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo&
HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo::operator=(
    HeaderDirectFromSellerSignals::ParseAndFindCompletedInfo&&) = default;

void HeaderDirectFromSellerSignals::ParseAndFindCompleted(
    ParseAndFindCompletedInfo info) const {
  scoped_refptr<HeaderDirectFromSellerSignals::Result> result;
  const auto it = results_.find(std::make_pair(info.origin, info.ad_slot));
  if (it != results_.end()) {
    result = it->second;
  }
  base::UmaHistogramTimes(
      "Ads.InterestGroup.NetHeaderResponse.HeaderDirectFromSellerSignals."
      "ParseAndFindMatchTime",
      base::TimeTicks::Now() - info.start_time);
  std::move(info.callback).Run(std::move(result));
}

void HeaderDirectFromSellerSignals::ProcessOneResponse(
    const data_decoder::DataDecoder::ValueOrError& result,
    const UnprocessedResponse& unprocessed_response,
    std::vector<std::string>& errors) {
  if (!result.has_value()) {
    errors.push_back(base::StringPrintf(
        "directFromSellerSignalsHeaderAdSlot: encountered invalid JSON: '%s' "
        "for Ad-Auction-Signals=%s",
        result.error().c_str(), unprocessed_response.response_json.c_str()));
    return;
  }

  const base::Value::List* maybe_list = result->GetIfList();
  if (!maybe_list) {
    errors.push_back(base::StringPrintf(
        "directFromSellerSignalsHeaderAdSlot: encountered response where "
        "top-level JSON value isn't an array: Ad-Auction-Signals=%s",
        unprocessed_response.response_json.c_str()));
    return;
  }

  std::set<std::string> ad_slots_from_response;
  for (const base::Value& list_item : *maybe_list) {
    const base::Value::Dict* maybe_dict = list_item.GetIfDict();
    if (!maybe_dict) {
      errors.push_back(
          base::StringPrintf("directFromSellerSignalsHeaderAdSlot: encountered "
                             "non-dict list item: Ad-AuctionSignals=%s",
                             unprocessed_response.response_json.c_str()));
      continue;
    }

    const std::string* maybe_ad_slot = maybe_dict->FindString("adSlot");
    if (!maybe_ad_slot) {
      errors.push_back(base::StringPrintf(
          "directFromSellerSignalsHeaderAdSlot: encountered dict without "
          "\"adSlot\" key: Ad-Auction-Signals=%s",
          unprocessed_response.response_json.c_str()));
      continue;
    }

    if (!ad_slots_from_response.insert(*maybe_ad_slot).second) {
      errors.push_back(base::StringPrintf(
          "directFromSellerSignalsHeaderAdSlot: encountered dict with "
          "duplicate adSlot key \"%s\": Ad-Auction-Signals=%s",
          maybe_ad_slot->c_str(), unprocessed_response.response_json.c_str()));
      continue;
    }

    const base::Value* maybe_seller_signals = maybe_dict->Find("sellerSignals");
    absl::optional<std::string> seller_signals;
    if (maybe_seller_signals) {
      seller_signals.emplace();
      JSONStringValueSerializer serializer(&seller_signals.value());
      if (!serializer.Serialize(*maybe_seller_signals)) {
        errors.push_back(base::StringPrintf(
            "directFromSellerSignalsHeaderAdSlot: failed to re-serialize "
            "sellerSignals: Ad-Auction-Signals=%s",
            unprocessed_response.response_json.c_str()));
        seller_signals.reset();
      }
    }

    const base::Value* maybe_auction_signals =
        maybe_dict->Find("auctionSignals");
    absl::optional<std::string> auction_signals;
    if (maybe_auction_signals) {
      auction_signals.emplace();
      JSONStringValueSerializer serializer(&auction_signals.value());
      if (!serializer.Serialize(*maybe_auction_signals)) {
        errors.push_back(base::StringPrintf(
            "directFromSellerSignalsHeaderAdSlot: failed to re-serialize "
            "auctionSignals: Ad-Auction-Signals=%s",
            unprocessed_response.response_json.c_str()));
        auction_signals.reset();
      }
    }

    std::vector<std::pair<url::Origin, std::string>> per_buyer_signals_vec;
    const base::Value::Dict* maybe_per_buyer_signals =
        maybe_dict->FindDict("perBuyerSignals");
    if (maybe_per_buyer_signals) {
      for (const std::pair<const std::string&, const base::Value&> item :
           *maybe_per_buyer_signals) {
        // Checking that the origin isn't opaque, untrustworthy, etc. shouldn't
        // be necessary, since such origins aren't allowed for the interest
        // group buyers. But, we check for https as a sanity check, which will
        // also fail if the origin isn't valid.
        url::Origin origin = url::Origin::Create(GURL(item.first));
        if (origin.scheme() != url::kHttpsScheme) {
          errors.push_back(base::StringPrintf(
              "directFromSellerSignalsHeaderAdSlot: encountered non-https "
              "perBuyerSignals origin '%s': Ad-Auction-Signals=%s",
              item.first.c_str(), unprocessed_response.response_json.c_str()));
          continue;
        }
        std::string origin_signals;
        JSONStringValueSerializer serializer(&origin_signals);
        if (serializer.Serialize(item.second)) {
          per_buyer_signals_vec.emplace_back(std::move(origin),
                                             std::move(origin_signals));
        } else {
          errors.push_back(base::StringPrintf(
              "directFromSellerSignalsHeaderAdSlot: failed to re-serialize "
              "perBuyerSignals[%s]: Ad-Auction-Signals=%s",
              item.first.c_str(), unprocessed_response.response_json.c_str()));
        }
      }
    }
    base::flat_map<url::Origin, std::string> per_buyer_signals(
        std::move(per_buyer_signals_vec));

    results_[std::make_pair(unprocessed_response.origin, *maybe_ad_slot)] =
        base::MakeRefCounted<HeaderDirectFromSellerSignals::Result>(
            std::move(seller_signals), std::move(auction_signals),
            std::move(per_buyer_signals));
  }
}

void HeaderDirectFromSellerSignals::OnJsonDecoded(
    data_decoder::DataDecoder& decoder,
    UnprocessedResponse current_unprocessed_response,
    std::vector<std::string> errors,
    data_decoder::DataDecoder::ValueOrError result) {
  ProcessOneResponse(result, current_unprocessed_response, errors);

  if (unprocessed_header_responses_.empty()) {
    std::move(add_witness_for_origin_completed_callback_)
        .Run(std::move(errors));
    while (!parse_and_find_completed_infos_.empty()) {
      ParseAndFindCompleted(std::move(parse_and_find_completed_infos_.front()));
      parse_and_find_completed_infos_.pop();
    }
    return;
  }

  DecodeNextResponse(decoder, std::move(errors));
}

void HeaderDirectFromSellerSignals::DecodeNextResponse(
    data_decoder::DataDecoder& decoder,
    std::vector<std::string> errors) {
  CHECK(!unprocessed_header_responses_.empty());

  UnprocessedResponse next_unprocessed_response =
      std::move(unprocessed_header_responses_.front());
  unprocessed_header_responses_.pop();

  // NOTE: The class comment for HeaderDirectFromSellerSignals requires that the
  // DataDecoder instances passed to AddWitnessForOrigin() be destroyed before
  // this HeaderDirectFromSellerSignals, so base::Unretained() below is safe.
  decoder.ParseJson(
      next_unprocessed_response.response_json,
      base::BindOnce(&HeaderDirectFromSellerSignals::OnJsonDecoded,
                     base::Unretained(this), std::ref(decoder),
                     next_unprocessed_response, std::move(errors)));
}

}  // namespace content
