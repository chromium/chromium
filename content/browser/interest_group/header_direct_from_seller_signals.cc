// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/header_direct_from_seller_signals.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Searches a single Ad-Auction-Signals response header, `original_str` (parsed
// as JSON into `result`) for the first encountered dictionary with the the
// adSlot value equal to `ad_slot`, returning signals from that dictionary if
// found, or null if none are found.
//
// Errors, if encountered, are appended to `errors`. Errors may be encountered
// even if signals matching the `ad_slot` are still ultimately found and
// returned.
std::unique_ptr<HeaderDirectFromSellerSignals> MaybeFindAdSlotSignalsInResponse(
    const data_decoder::DataDecoder::ValueOrError& result,
    const std::string& ad_slot,
    const std::string& original_str,
    std::vector<std::string>& errors) {
  if (!result.has_value()) {
    errors.push_back(base::StringPrintf(
        "When looking for directFromSellerSignalsHeaderAdSlot %s, encountered "
        "invalid JSON: '%s' for Ad-Auction-Signals=%s",
        ad_slot.c_str(), result.error().c_str(), original_str.c_str()));
    return nullptr;
  }

  const base::Value::List* maybe_list = result->GetIfList();
  if (!maybe_list) {
    errors.push_back(base::StringPrintf(
        "When looking for directFromSellerSignalsHeaderAdSlot %s, encountered "
        "response where top-level JSON value isn't an array: "
        "Ad-Auction-Signals=%s",
        ad_slot.c_str(), original_str.c_str()));
    return nullptr;
  }

  for (const base::Value& list_item : *maybe_list) {
    const base::Value::Dict* maybe_dict = list_item.GetIfDict();
    if (!maybe_dict) {
      errors.push_back(base::StringPrintf(
          "When looking for directFromSellerSignalsHeaderAdSlot %s, "
          "encountered non-dict list item: Ad-AuctionSignals=%s",
          ad_slot.c_str(), original_str.c_str()));
      continue;
    }

    const std::string* maybe_ad_slot = maybe_dict->FindString("adSlot");
    if (!maybe_ad_slot) {
      errors.push_back(base::StringPrintf(
          "When looking for directFromSellerSignalsHeaderAdSlot %s, "
          "encountered dict without \"adSlot\" key: Ad-Auction-Signals=%s",
          ad_slot.c_str(), original_str.c_str()));
      continue;
    }

    if (*maybe_ad_slot != ad_slot) {
      continue;
    }

    const base::Value* maybe_seller_signals = maybe_dict->Find("sellerSignals");
    absl::optional<std::string> seller_signals;
    if (maybe_seller_signals) {
      seller_signals.emplace();
      JSONStringValueSerializer serializer(&seller_signals.value());
      if (!serializer.Serialize(*maybe_seller_signals)) {
        errors.push_back(base::StringPrintf(
            "When looking for directFromSellerSignalsHeaderAdSlot %s, failed "
            "to re-serialize sellerSignals: Ad-Auction-Signals=%s",
            ad_slot.c_str(), original_str.c_str()));
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
            "When looking for directFromSellerSignalsHeaderAdSlot %s, failed "
            "to re-serialize auctionSignals: Ad-Auction-Signals=%s",
            ad_slot.c_str(), original_str.c_str()));
        auction_signals.reset();
      }
    }

    base::flat_map<url::Origin, std::string> per_buyer_signals;
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
              "When looking for directFromSellerSignalsHeaderAdSlot %s, "
              "encountered non-https perBuyerSignals origin '%s': "
              "Ad-Auction-Signals=%s",
              ad_slot.c_str(), item.first.c_str(), original_str.c_str()));
          continue;
        }
        std::string origin_signals;
        JSONStringValueSerializer serializer(&origin_signals);
        if (serializer.Serialize(item.second)) {
          per_buyer_signals[origin] = origin_signals;
        } else {
          errors.push_back(base::StringPrintf(
              "When looking for directFromSellerSignalsHeaderAdSlot %s, failed "
              "to re-serialize perBuyerSignals[%s]: Ad-Auction-Signals=%s",
              ad_slot.c_str(), item.first.c_str(), original_str.c_str()));
        }
      }
    }

    return std::make_unique<HeaderDirectFromSellerSignals>(
        std::move(seller_signals), std::move(auction_signals),
        std::move(per_buyer_signals));
  }

  return nullptr;
}

std::string NoMatchError(std::string ad_slot) {
  return base::StringPrintf(
      "When looking for directFromSellerSignalsHeaderAdSlot %s, failed to "
      "find a matching response.",
      ad_slot.c_str());
}

// Searches for a signals dict whose adSlot value matches `ad_slot`, continuing
// to the next response if no match is found.
void OnJsonDecoded(
    HeaderDirectFromSellerSignals::GetDecoderCallback get_decoder,
    std::unique_ptr<const std::set<std::string>> responses,
    std::set<std::string>::iterator it,
    std::string ad_slot,
    std::vector<std::string> errors,
    HeaderDirectFromSellerSignals::CompletionCallback callback,
    base::TimeTicks start_time,
    data_decoder::DataDecoder::ValueOrError result) {
  std::unique_ptr<HeaderDirectFromSellerSignals> maybe_parsed =
      MaybeFindAdSlotSignalsInResponse(result, ad_slot, *it, errors);
  if (maybe_parsed) {
    // Found a match.
    base::UmaHistogramTimes(
        "Ads.InterestGroup.NetHeaderResponse.HeaderDirectFromSellerSignals."
        "ParseAndFindMatchTime",
        base::TimeTicks::Now() - start_time);
    std::move(callback).Run(std::move(maybe_parsed), std::move(errors));
    return;
  }

  ++it;
  if (it == responses->end()) {
    // No responses matched so add an error and return.
    base::UmaHistogramTimes(
        "Ads.InterestGroup.NetHeaderResponse.HeaderDirectFromSellerSignals."
        "ParseAndFindMatchTime",
        base::TimeTicks::Now() - start_time);
    errors.push_back(NoMatchError(ad_slot));
    std::move(callback).Run(std::make_unique<HeaderDirectFromSellerSignals>(),
                            std::move(errors));
    return;
  }

  // Decode the next response.
  data_decoder::DataDecoder* maybe_decoder = get_decoder.Run();
  if (!maybe_decoder) {
    return;
  }

  maybe_decoder->ParseJson(
      *it, base::BindOnce(&OnJsonDecoded, std::move(get_decoder),
                          std::move(responses), it, std::move(ad_slot),
                          std::move(errors), std::move(callback), start_time));
}

}  // namespace

HeaderDirectFromSellerSignals::HeaderDirectFromSellerSignals() = default;

HeaderDirectFromSellerSignals::~HeaderDirectFromSellerSignals() = default;

// TODO(crbug.com/1462720): Add UMA for response size.
/* static */
void HeaderDirectFromSellerSignals::ParseAndFind(
    GetDecoderCallback get_decoder,
    const std::set<std::string>& responses,
    std::string ad_slot,
    CompletionCallback callback) {
  std::vector<std::string> errors;
  if (responses.empty()) {
    base::UmaHistogramTimes(
        "Ads.InterestGroup.NetHeaderResponse.HeaderDirectFromSellerSignals."
        "ParseAndFindMatchTime",
        base::Seconds(0));
    errors.push_back(NoMatchError(ad_slot));
    std::move(callback).Run(std::make_unique<HeaderDirectFromSellerSignals>(),
                            std::move(errors));
    return;
  }

  // The decoding is asynchronous, and it's possible that the original
  // `responses` may have been destroyed before parsing completes; therefore, a
  // copy is required.
  auto my_responses = std::make_unique<const std::set<std::string>>(responses);
  auto it = my_responses->begin();
  data_decoder::DataDecoder* maybe_decoder = get_decoder.Run();
  if (!maybe_decoder) {
    return;
  }
  maybe_decoder->ParseJson(
      *it, base::BindOnce(&OnJsonDecoded, get_decoder, std::move(my_responses),
                          it, std::move(ad_slot), std::move(errors),
                          std::move(callback), base::TimeTicks::Now()));
}

HeaderDirectFromSellerSignals::HeaderDirectFromSellerSignals(
    absl::optional<std::string> seller_signals,
    absl::optional<std::string> auction_signals,
    base::flat_map<url::Origin, std::string> per_buyer_signals)
    : seller_signals_(std::move(seller_signals)),
      auction_signals_(std::move(auction_signals)),
      per_buyer_signals_(std::move(per_buyer_signals)) {}

}  // namespace content
