// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TRUSTED_SIGNALS_URL_BUILDER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TRUSTED_SIGNALS_URL_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/cpp/creative_info.h"
#include "url/gurl.h"

namespace auction_worklet {

class TrustedBiddingSignalsUrlBuilder;
class TrustedScoringSignalsUrlBuilder;

// Manages building and loading trusted signals kvv1 URLs. Provides a shared
// interface for bidding signals URLs and scoring signals URLs.
//
// Create a TrustedBiddingSignalsUrlBuilder to build a bidding signals URL or a
// TrustedScoringSignalsUrlBuilder to build a scoring signals url. Then for each
// bidding or scoring request with the same constructor parameters, call
// TryToAddRequest(), which will attempt to include the associated values in the
// URL, if we're able to do so while respecting all
// `max_trusted_signals_url_length` limits. Once done building the URL (either
// because there are no more values to include or because we've reached a length
// limit (i.e. TryToAddRequest() failed)), compose the URL with ComposeURL().
// The class can be reused to build a new URL by calling Reset().
class CONTENT_EXPORT TrustedSignalsUrlBuilder {
 public:
  TrustedSignalsUrlBuilder& operator=(const TrustedSignalsUrlBuilder&) = delete;
  TrustedSignalsUrlBuilder(const TrustedSignalsUrlBuilder&) = delete;

  virtual ~TrustedSignalsUrlBuilder();

  // Compose the URL. This function should only be called once we're ready to
  // retrieve a full URL, because it destroys the intermediate URL fragments
  // held by this class by moving them into the new URL. Reset() must be called
  // before using the TrustedSignalsUrlBuilder to build another URL.
  GURL ComposeURL();

  // Reset the builder so that it can be used to build another URL.
  // `max_trusted_signals_url_length` values from previous TryToAddRequest()
  // calls will no longer apply.
  void Reset();

  // Extract the attributes needed to build and create trusted bidding signals.
  // Requests may not be added after these methods have been invoked, without
  // calling Reset() first.
  std::set<std::string> TakeInterestGroupNames();
  std::set<std::string> TakeBiddingSignalsKeys();

  // Extract the attributes needed to build and create trusted scoring signals.
  // Requests may not be added after these methods have been invoked, without
  // calling Reset() first.
  std::set<CreativeInfo> TakeAds();
  std::set<CreativeInfo> TakeAdComponents();

 protected:
  // Note: this is in the order these are assembled into the URL string.
  enum class UrlField {
    kBase,
    kKeys,
    kInterestGroupNames,
    kRenderUrls,
    kAdComponentRenderUrls,
    kExperimentGroupId,
    kSlotSizeParam,
    kAdCreativeScanningMetadata,
    kAdComponentCreativeScanningMetadata,
    kAdSizes,
    kAdComponentSizes,
    kAdBuyer,
    kAdComponentBuyer,
    kBuyerAndSellerReportingIds,
    kNumValues
  };

  // A portion of a URL. Those with the same value of `field` are meant to be
  // appended right next to each other.
  struct UrlPiece {
    UrlField field;
    std::string text;

    friend bool operator==(const UrlPiece&, const UrlPiece&) = default;
  };

  TrustedSignalsUrlBuilder(std::string hostname,
                           GURL trusted_signals_url,
                           std::optional<uint16_t> experiment_group_id);

  // This method should be called after updating `main_fragments_` and
  // `aux_fragments_` to incorporate a request, with
  // `initial_num_main_fragments` and `initial_num_aux_fragments` giving the
  // size of those two vectors before the incorporation.
  //
  // If the resulting URL is within various size limits, updates the object's
  // size-tracking state and return true.
  //
  // If the resulting URL is too large, rolls back the changes to
  // `main_fragments_` and `aux_fragments_`, and returns false, denoting that
  // `request` should not be included in this batch.
  bool CommitOrRollback(size_t initial_num_main_fragments,
                        size_t initial_num_aux_fragments,
                        size_t max_trusted_signals_url_length);

 private:
  friend class TrustedBiddingSignalsUrlBuilder;
  friend class TrustedScoringSignalsUrlBuilder;

  const std::string hostname_;

  const GURL trusted_signals_url_;

  const std::optional<uint16_t> experiment_group_id_;

  // True if we've added a request with TryToAddRequest() and haven't Reset()
  // since.
  bool added_first_request_ = false;

  // The maximum allowed length of a URL with this group of requests.
  size_t length_limit_ = std::numeric_limits<size_t>::max();

  // Parameters for building a bidding signals URL.
  std::set<std::string> interest_group_names_;
  std::set<std::string> bidding_signals_keys_;

  // Parameters for building a scoring signals URL.
  std::set<CreativeInfo> ads_;
  std::set<CreativeInfo> ad_components_;

  // Portions of incrementally composed URL, and how long the current
  // portion is.
  std::vector<UrlPiece> main_fragments_;
  std::vector<UrlPiece> aux_fragments_;
  size_t length_thus_far_ = 0;
};

class CONTENT_EXPORT TrustedBiddingSignalsUrlBuilder
    : public TrustedSignalsUrlBuilder {
 public:
  TrustedBiddingSignalsUrlBuilder(
      std::string hostname,
      GURL trusted_signals_url,
      std::optional<uint16_t> experiment_group_id,
      std::string trusted_bidding_signals_slot_size_param);

  TrustedBiddingSignalsUrlBuilder& operator=(
      const TrustedBiddingSignalsUrlBuilder&) = delete;
  TrustedBiddingSignalsUrlBuilder(const TrustedBiddingSignalsUrlBuilder&) =
      delete;

  ~TrustedBiddingSignalsUrlBuilder() override = default;

  // Try to add these new attributes to the URL we're building. If the new URL
  // (including the new `interest_group_name` and `bidder_keys`) would exceed
  // `max_trusted_signals_url_length` or a
  // `max_trusted_signals_url_length` for a previous request, return false.
  // Otherwise, include these values in the URL we're building and return true.
  bool TryToAddRequest(const std::string& interest_group_name,
                       const std::set<std::string>& bidder_keys,
                       size_t max_trusted_signals_url_length);

 private:
  // BuildTrusted{Bidding,Scoring}SignalsURL helps incrementally
  // compute trusted signals URLs merged from multiple requests by accumulating
  // fragments of the full URL into the `..._fragments_` members.

  // For BuildTrustedBiddingSignalsURL, `main_fragments_` is used to accumulate
  // all of URL pieces that do not have to do with the `keys` parameter, which
  // go into `aux_fragments_`. This split is needed because the first request
  // (or all) requests might have an empty `bidding_signals_keys`.
  void BuildTrustedBiddingSignalsURL(
      const std::set<std::string>& new_interest_group_names,
      const std::set<std::string>& new_bidding_signals_keys);

  const std::string trusted_bidding_signals_slot_size_param_;
};

class CONTENT_EXPORT TrustedScoringSignalsUrlBuilder
    : public TrustedSignalsUrlBuilder {
 public:
  TrustedScoringSignalsUrlBuilder(std::string hostname,
                                  GURL trusted_signals_url,
                                  std::optional<uint16_t> experiment_group_id,
                                  bool send_creative_scanning_metadata);

  TrustedScoringSignalsUrlBuilder& operator=(
      const TrustedScoringSignalsUrlBuilder&) = delete;
  TrustedScoringSignalsUrlBuilder(const TrustedScoringSignalsUrlBuilder&) =
      delete;

  ~TrustedScoringSignalsUrlBuilder() override = default;

  // Try to add these new attributes to the URL we're building. If the new URL
  // (including the new `ad` and `ad_components`) would exceed
  // `max_trusted_signals_url_length` or a
  // `max_trusted_signals_url_length` for a previous request, return false.
  // Otherwise, include these values in the URL we're building and return true.
  bool TryToAddRequest(const CreativeInfo& ad,
                       const std::set<CreativeInfo>& ad_components,
                       size_t max_trusted_signals_url_length);

 private:
  // The equivalent of BuildTrustedBiddingSignalsURL for scoring.
  //
  // `ads` and `component_ads` are set<CreativeInfo>
  // rather than map<GURL, something> because the same URL can have different
  // creative scanning metadata in different IGs, or different size in
  // difference occurrences as a component ad, etc.
  void BuildTrustedScoringSignalsURL(
      const std::set<CreativeInfo>& new_ads,
      const std::set<CreativeInfo>& new_component_ads);

  const bool send_creative_scanning_metadata_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TRUSTED_SIGNALS_URL_BUILDER_H_
