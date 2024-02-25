// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/origin.h"

namespace content {

// Parses the results of Ad-Auction-Signals header values, as provided to
// AddWitnessForOrigin().
//
// NOTE: The DataDecoder instances passed to AddWitnessForOrigin() should be
// destroyed before destroying this class.
//
// JSON format is described in https://github.com/WICG/turtledove/pull/695.
class CONTENT_EXPORT HeaderDirectFromSellerSignals {
 public:
  // Signals for a given origin, ad_slot tuple -- if no match was found, a
  // default-constructed Result is returned.
  //
  // RefCounted so that reporting worklet functions may continue to access these
  // data, even after navigating away to another page (AdAuctionPageData owns
  // HeaderDirectFromSellerSignals).
  class CONTENT_EXPORT Result : public base::RefCounted<Result> {
   public:
    Result();

    Result(std::optional<std::string> seller_signals,
           std::optional<std::string> auction_signals,
           base::flat_map<url::Origin, std::string> per_buyer_signals);

    Result(Result&) = delete;
    Result& operator=(Result&) = delete;

    // Results of the `sellerSignals` JSON dictionary field.
    const std::optional<std::string>& seller_signals() const {
      return seller_signals_;
    }

    // Results of the `auctionSignals` JSON dictionary field.
    const std::optional<std::string>& auction_signals() const {
      return auction_signals_;
    }

    // Results of the `perBuyerSignals` JSON dictionary field.
    const base::flat_map<url::Origin, std::string>& per_buyer_signals() const {
      return per_buyer_signals_;
    }

   private:
    friend class base::RefCounted<Result>;
    ~Result();

    const std::optional<std::string> seller_signals_;
    const std::optional<std::string> auction_signals_;
    const base::flat_map<url::Origin, std::string> per_buyer_signals_;
  };

  // Returns the result. The HeaderDirectFromSellerSignals::Result pointer will
  // be null if no match was found.
  using ParseAndFindCompletedCallback = base::OnceCallback<void(
      scoped_refptr<HeaderDirectFromSellerSignals::Result>)>;

  // Called when AddWitnessForOrigin() completes, passing a vector of error
  // strings that occurred during JSON parsing, if any.
  using AddWitnessForOriginCompletedCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  HeaderDirectFromSellerSignals();
  ~HeaderDirectFromSellerSignals();

  HeaderDirectFromSellerSignals(HeaderDirectFromSellerSignals&) = delete;
  HeaderDirectFromSellerSignals& operator=(HeaderDirectFromSellerSignals&) =
      delete;

  // Asynchronously parses the captured JSON responses -- each of which should
  // represent an array of dictionaries -- until such a dictionary is found that
  // matches the adSlot specified by `ad_slot` from the origin `origin`. Results
  // are provided to `callback` -- if no match is found, null will be passed.
  //
  // NOTE: No decoder need be passed since parsing of received JSON responses
  // starts as soon as they are received, in AddWitnessForOrigin() --
  // ParseAndFind() waits until such parsing is done, if it hasn't already
  // completed.
  //
  // NOTE: This method may return synchronously if all received responses have
  // already been parsed before the ParseAndFind() call begins.
  //
  // NOTE: `callback` will not be invoked if a DataDecoder passed to
  // AddWitnessForOrigin() is destroyed before parsing using that decoder
  // completes.
  void ParseAndFind(const url::Origin& origin,
                    const std::string& ad_slot,
                    ParseAndFindCompletedCallback callback);

  // Called every time an Ad-Auction-Signals response `response` is captured,
  // where `origin` is origin that served the response.
  //
  // NOTE: `callback` will not be invoked if DataDecoder is destroyed during
  // processing. Also, if multiple AddWitnessForOrigin() calls are in-flight at
  // the same time, only the `callback` for the first call will be invoked.
  void AddWitnessForOrigin(data_decoder::DataDecoder& decoder,
                           const url::Origin& origin,
                           const std::string& response,
                           AddWitnessForOriginCompletedCallback callback);

 private:
  // Represents signals origin, ad_slot.
  using ResultsKey = std::pair<url::Origin, std::string>;

  // A single Ad-Auction-Signals response captured from `origin`.
  struct UnprocessedResponse {
    // The origin that served the Ad-Auction-Signals response `response`.
    url::Origin origin;

    // The Ad-Auction-Signals response served by `origin`.
    std::string response_json;
  };

  // Information from ParseAndFind() calls used by ParseAndFindCompleted.
  struct ParseAndFindCompletedInfo {
    ParseAndFindCompletedInfo(base::TimeTicks start_time,
                              url::Origin origin,
                              std::string ad_slot,
                              ParseAndFindCompletedCallback callback);
    ~ParseAndFindCompletedInfo();

    ParseAndFindCompletedInfo(ParseAndFindCompletedInfo&&);
    ParseAndFindCompletedInfo& operator=(ParseAndFindCompletedInfo&&);

    // The time ParseAndFind() was called.
    base::TimeTicks start_time;

    // The origin that responded with the signals.
    url::Origin origin;

    // The adSlot key to find.
    std::string ad_slot;

    // The completion callback passed to ParseAndFind().
    ParseAndFindCompletedCallback callback;
  };

  // Called for each ParseAndFind() call when all all `unprocessed_responses_`
  // have been parsed and results have been placed into `results_`.
  void ParseAndFindCompleted(ParseAndFindCompletedInfo info) const;

  // Processes a single Ad-Auction-Signals response header,
  // `unprocessed_response` (parsed as JSON into `result`), updating `results_`
  // as valid signals are encountered.
  //
  // Errors, if encountered, are appended to `errors`. Errors may be
  // encountered even if some valid signals are added to `results_`.
  void ProcessOneResponse(const data_decoder::DataDecoder::ValueOrError& result,
                          const UnprocessedResponse& unprocessed_response,
                          std::vector<std::string>& errors);

  // Calls ProcessOneResponse(), runs callbacks on completion, and otherwise
  // calls DecodeNextResponse() to continue parsing the next
  // UnprocessedResponse, if there is one.
  void OnJsonDecoded(data_decoder::DataDecoder& decoder,
                     UnprocessedResponse current_unprocessed_response,
                     std::vector<std::string> errors,
                     base::TimeTicks parse_start_time,
                     data_decoder::DataDecoder::ValueOrError result);

  // Start decoding the next UnprocessedResponse in `unprocessed_responses_`
  // (CHECK()s that at least one is there). OnJsonDecoded() will be called when
  // decoding completes.
  void DecodeNextResponse(data_decoder::DataDecoder& decoder,
                          std::vector<std::string> errors);

  // A raw queue of responses added by AddAuctionSignalsWitnessForOrigin(). Upon
  // insertion into this queue, the unprocessed raw responses are removed and
  // parsed as JSON, and the results are put in the `results_` map, potentially
  // overwriting values for keys in that map.
  //
  // NOTE: The queue *may* be empty even though processing is still in-process
  // (since entries that are being processed are first removed from the queue)!
  // To determine if responses are already being processed, check if
  // `add_witness_for_origin_completed_callback_` is non-null.
  base::queue<UnprocessedResponse> unprocessed_header_responses_;

  // Callback for AddWitnessForOrigin() called after processing of the remaining
  // `unprocessed_header_responses_` completes.
  //
  // Will be a non-null callback *iff* processing is currently in process.
  AddWitnessForOriginCompletedCallback
      add_witness_for_origin_completed_callback_;

  // Parameters for ParseAndFindCompleted() waiting on on processing the
  // remaining responses in `unprocessed_header_responses_`. This allows only
  // having a single concurrent processor of `unprocessed_header_responses_`,
  // ensuring that processing completes in the correct order.
  base::queue<ParseAndFindCompletedInfo> parse_and_find_completed_infos_;

  // Processed responses, keyed by the composite key origin, ad_slot. The
  // unprocessed responses are processed in a manner such that the most recent
  // response value for a given key overwrites older values.
  //
  // std::map is chosen over base::flat_map since the latter doesn't support
  // efficient bulk insertion after construction with the desired overwriting
  // behavior.
  std::map<ResultsKey, scoped_refptr<HeaderDirectFromSellerSignals::Result>>
      results_;

  // For metrics -- the total amount of response bytes processed from
  // `unprocessed_header_responses_` since the last time
  // `unprocessed_header_responses_` was empty.
  size_t processed_bytes_per_round_ = 0u;

  // For metrics -- the last moment we started processing JSON responses.
  base::TimeTicks last_round_started_time_ = base::TimeTicks::Min();

  // For metrics -- the number of AddWitnessForOrigin() calls.
  size_t num_add_witness_for_origin_calls_ = 0u;

  // For metrics -- the number of ParseAndFind() calls.
  size_t parse_and_find_calls_ = 0u;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_HEADER_DIRECT_FROM_SELLER_SIGNALS_H_
