// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/url_constants.h"

namespace blink {

namespace {

const size_t kMaxAdRenderIdSize = 12;

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const KURL& url) {
  if (!url.IsValid() || !url.ProtocolIs(url::kHttpsScheme)) {
    return false;
  }

  return url.User().empty() && url.Pass().empty();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, or update URL. Ad render URLs should be checked with
// IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check, and allows references.
bool IsUrlAllowed(const KURL& url, const mojom::blink::InterestGroup& group) {
  if (!group.owner->IsSameOriginWith(SecurityOrigin::Create(url).get())) {
    return false;
  }

  return IsUrlAllowedForRenderUrls(url) && !url.HasFragmentIdentifier();
}

// Checks if `url` can be used with interest group `group` as a trusted
// bidding signals URL. Sets `error_out` to an error message appropriate for
// failure regardless of success or failure.
bool IsUrlAllowedForTrustedBiddingSignals(
    const KURL& url,
    const mojom::blink::InterestGroup& group,
    String& error_out) {
  bool allow_cross_origin = base::FeatureList::IsEnabled(
      blink::features::kFledgePermitCrossOriginTrustedSignals);
  if (allow_cross_origin) {
    error_out =
        "trustedBiddingSignalsURL must have https schema and have no query "
        "string, fragment identifier or embedded credentials.";
  } else {
    error_out =
        "trustedBiddingSignalsURL must have the same origin as the "
        "InterestGroup owner and have no query string, fragment identifier or "
        "embedded credentials.";
  }

  if (!IsUrlAllowedForRenderUrls(url) || url.HasFragmentIdentifier() ||
      !group.trusted_bidding_signals_url->Query().IsNull()) {
    return false;
  }

  if (allow_cross_origin) {
    return true;
  } else {
    return group.owner->IsSameOriginWith(SecurityOrigin::Create(url).get());
  }
}

size_t EstimateHashMapSize(const HashMap<String, double>& hash_map) {
  size_t result = 0;
  for (const auto& pair : hash_map) {
    result += pair.key.length() + sizeof(pair.value);
  }
  return result;
}

}  // namespace

// The logic in this method must be kept in sync with
// InterestGroup::EstimateSize() in blink/common/interest_group/.
size_t EstimateBlinkInterestGroupSize(
    const mojom::blink::InterestGroup& group) {
  size_t size = 0u;
  size += group.owner->ToString().length();
  size += group.name.length();
  size += sizeof(group.priority);
  size += sizeof(group.execution_mode);
  size += sizeof(group.enable_bidding_signals_prioritization);

  if (group.priority_vector) {
    size += EstimateHashMapSize(*group.priority_vector);
  }
  if (group.priority_signals_overrides) {
    size += EstimateHashMapSize(*group.priority_signals_overrides);
  }
  // Tests ensure this matches the blink::InterestGroup size, which is computed
  // from the underlying number of enum bytes (the actual size on disk will
  // vary, but we need a rough estimate for size enforcement).
  constexpr size_t kCapabilitiesFlagsSize = 4;
  if (group.seller_capabilities) {
    for (const auto& [seller_origin, flags] : *group.seller_capabilities) {
      size += seller_origin->ToString().length() + kCapabilitiesFlagsSize;
    }
  }
  size += kCapabilitiesFlagsSize;  // For all_sellers_capabilities.
  if (group.bidding_url) {
    size += group.bidding_url->GetString().length();
  }

  if (group.bidding_wasm_helper_url) {
    size += group.bidding_wasm_helper_url->GetString().length();
  }

  if (group.update_url) {
    size += group.update_url->GetString().length();
  }

  if (group.trusted_bidding_signals_url) {
    size += group.trusted_bidding_signals_url->GetString().length();
  }
  if (group.trusted_bidding_signals_keys) {
    for (const String& key : *group.trusted_bidding_signals_keys) {
      size += key.length();
    }
  }
  size += sizeof(group.trusted_bidding_signals_slot_size_mode);
  size += sizeof(group.max_trusted_bidding_signals_url_length);
  if (group.trusted_bidding_signals_coordinator) {
    size += group.trusted_bidding_signals_coordinator->ToString().length();
  }
  size += group.user_bidding_signals.length();

  if (group.ads) {
    for (const auto& ad : group.ads.value()) {
      size += ad->render_url.length();
      size += ad->size_group.length();
      size += ad->buyer_reporting_id.length();
      size += ad->buyer_and_seller_reporting_id.length();
      if (ad->selectable_buyer_and_seller_reporting_ids) {
        for (const auto& id : *ad->selectable_buyer_and_seller_reporting_ids) {
          size += id.length();
        }
      }
      size += ad->metadata.length();
      size += ad->ad_render_id.length();
      if (ad->allowed_reporting_origins) {
        for (const auto& origin : ad->allowed_reporting_origins.value()) {
          size += origin->ToString().length();
        }
      }
    }
  }

  if (group.ad_components) {
    for (const auto& ad : group.ad_components.value()) {
      size += ad->render_url.length();
      size += ad->size_group.length();
      size += ad->metadata.length();
      size += ad->ad_render_id.length();
    }
  }

  if (group.ad_sizes) {
    for (const auto& [size_name, size_obj] : group.ad_sizes.value()) {
      size += size_name.length();
      size += sizeof(size_obj->width);
      size += sizeof(size_obj->height);
      size += sizeof(size_obj->width_units);
      size += sizeof(size_obj->height_units);
    }
  }

  if (group.size_groups) {
    for (const auto& [group_name, size_list] : group.size_groups.value()) {
      size += group_name.length();
      for (const auto& size_name : size_list) {
        size += size_name.length();
      }
    }
  }
  constexpr size_t kAuctionServerRequestFlagsSize = 4;
  size += kAuctionServerRequestFlagsSize;

  if (group.additional_bid_key) {
    size += X25519_PUBLIC_VALUE_LEN;
  }

  if (group.aggregation_coordinator_origin) {
    size += group.aggregation_coordinator_origin->ToString().length();
  }

  return size;
}

// The logic in this method must be kept in sync with InterestGroup::IsValid()
// in blink/common/interest_group/.
bool ValidateBlinkInterestGroup(const mojom::blink::InterestGroup& group,
                                String& error_field_name,
                                String& error_field_value,
                                String& error) {
  if (group.owner->Protocol() != url::kHttpsScheme) {
    error_field_name = "owner";
    error_field_value = group.owner->ToString();
    error = "owner origin must be HTTPS.";
    return false;
  }

  // Checking for finiteness for priority, priority_vector, and
  // priority_signals_overrides is performed by WebIDL.

  // This check is here to keep it in sync with InterestGroup::IsValid(), but
  // checks in navigator_auction.cc should ensure the execution mode is always
  // valid.
  if (group.execution_mode !=
          mojom::blink::InterestGroup::ExecutionMode::kCompatibilityMode &&
      group.execution_mode !=
          mojom::blink::InterestGroup::ExecutionMode::kGroupedByOriginMode &&
      group.execution_mode !=
          mojom::blink::InterestGroup::ExecutionMode::kFrozenContext) {
    error_field_name = "executionMode";
    error_field_value = String::Number(static_cast<int>(group.execution_mode));
    error = "execution mode is not valid.";
    return false;
  }

  if (group.seller_capabilities) {
    for (const auto& [seller_origin, flags] : *group.seller_capabilities) {
      if (seller_origin->Protocol() != url::kHttpsScheme) {
        error_field_name = "sellerCapabilities";
        error_field_value = seller_origin->ToString();
        error = "sellerCapabilities origins must all be HTTPS.";
        return false;
      }
    }
  }

  if (group.bidding_url) {
    if (!IsUrlAllowed(*group.bidding_url, group)) {
      error_field_name = "biddingLogicURL";
      error_field_value = group.bidding_url->GetString();
      error =
          "biddingLogicURL must have the same origin as the InterestGroup "
          "owner and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.bidding_wasm_helper_url) {
    if (!IsUrlAllowed(*group.bidding_wasm_helper_url, group)) {
      error_field_name = "biddingWasmHelperURL";
      error_field_value = group.bidding_wasm_helper_url->GetString();
      error =
          "biddingWasmHelperURL must have the same origin as the InterestGroup "
          "owner and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.update_url) {
    if (!IsUrlAllowed(*group.update_url, group)) {
      error_field_name = "updateURL";
      error_field_value = group.update_url->GetString();
      error =
          "updateURL must have the same origin as the InterestGroup owner "
          "and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.trusted_bidding_signals_url) {
    // In addition to passing the same checks used on the other URLs,
    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    String trusted_url_error;
    if (!IsUrlAllowedForTrustedBiddingSignals(
            *group.trusted_bidding_signals_url, group, trusted_url_error) ||
        !group.trusted_bidding_signals_url->Query().IsNull()) {
      error_field_name = "trustedBiddingSignalsURL";
      error_field_value = group.trusted_bidding_signals_url->GetString();
      error = std::move(trusted_url_error);
      return false;
    }
  }

  // This check is here to keep it in sync with InterestGroup::IsValid(), but
  // checks in navigator_auction.cc should ensure the execution mode is always
  // valid.
  if (group.trusted_bidding_signals_slot_size_mode !=
          mojom::blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kNone &&
      group.trusted_bidding_signals_slot_size_mode !=
          mojom::blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kSlotSize &&
      group.trusted_bidding_signals_slot_size_mode !=
          mojom::blink::InterestGroup::TrustedBiddingSignalsSlotSizeMode::
              kAllSlotsRequestedSizes) {
    error_field_name = "trustedBiddingSignalsSlotSizeMode";
    error_field_value = String::Number(
        static_cast<int>(group.trusted_bidding_signals_slot_size_mode));
    error = "trustedBiddingSignalsSlotSizeMode is not valid.";
    return false;
  }

  // This check is here to keep it in sync with InterestGroup::IsValid().
  if (group.max_trusted_bidding_signals_url_length < 0) {
    error_field_name = "maxTrustedBiddingSignalsURLLength";
    error_field_value =
        String::Number(group.max_trusted_bidding_signals_url_length);
    error = "maxTrustedBiddingSignalsURLLength is negative.";
    return false;
  }

  if (group.trusted_bidding_signals_coordinator) {
    if (group.trusted_bidding_signals_coordinator->Protocol() !=
        url::kHttpsScheme) {
      error_field_name = "trustedBiddingSignalsCoordinator";
      error_field_value = group.trusted_bidding_signals_coordinator->ToString();
      error = "trustedBiddingSignalsCoordinator origin must be HTTPS.";
      return false;
    }
  }

  if (group.ads) {
    for (WTF::wtf_size_t i = 0; i < group.ads.value().size(); ++i) {
      const KURL& render_url = KURL(group.ads.value()[i]->render_url);
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("ads[%u].renderURL", i);
        error_field_value = render_url.GetString();
        error = "renderURLs must be HTTPS and have no embedded credentials.";
        return false;
      }
      const WTF::String& ad_size_group = group.ads.value()[i]->size_group;
      if (!ad_size_group.IsNull()) {
        if (ad_size_group.empty()) {
          error_field_name = String::Format("ads[%u].sizeGroup", i);
          error_field_value = ad_size_group;
          error = "Size group name cannot be empty.";
          return false;
        }
        if (!group.size_groups || !group.size_groups->Contains(ad_size_group)) {
          error_field_name = String::Format("ads[%u].sizeGroup", i);
          error_field_value = ad_size_group;
          error = "The assigned size group does not exist in sizeGroups map.";
          return false;
        }
      }
      if (group.ads.value()[i]->ad_render_id.length() > kMaxAdRenderIdSize) {
        error_field_name = String::Format("ads[%u].adRenderId", i);
        error_field_value = group.ads.value()[i]->ad_render_id;
        error = "The adRenderId is too long.";
        return false;
      }
      auto& allowed_reporting_origins =
          group.ads.value()[i]->allowed_reporting_origins;
      if (allowed_reporting_origins) {
        if (allowed_reporting_origins->size() >
            mojom::blink::kMaxAllowedReportingOrigins) {
          error_field_name =
              String::Format("ads[%u].allowedReportingOrigins", i);
          error_field_value = "";
          error = String::Format(
              "allowedReportingOrigins cannot have more than %hu elements.",
              mojom::blink::kMaxAllowedReportingOrigins);
          return false;
        }
        for (WTF::wtf_size_t j = 0; j < allowed_reporting_origins->size();
             ++j) {
          if (allowed_reporting_origins.value()[j]->Protocol() !=
              url::kHttpsScheme) {
            error_field_name =
                String::Format("ads[%u].allowedReportingOrigins", i);
            error_field_value =
                allowed_reporting_origins.value()[j]->ToString();
            error = "allowedReportingOrigins must all be HTTPS.";
            return false;
          }
        }
      }
    }
  }

  if (group.ad_components) {
    for (WTF::wtf_size_t i = 0; i < group.ad_components.value().size(); ++i) {
      const KURL& render_url = KURL(group.ad_components.value()[i]->render_url);
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("adComponents[%u].renderURL", i);
        error_field_value = render_url.GetString();
        error = "renderURLs must be HTTPS and have no embedded credentials.";
        return false;
      }
      const WTF::String& ad_component_size_group =
          group.ad_components.value()[i]->size_group;
      if (!ad_component_size_group.IsNull()) {
        if (ad_component_size_group.empty()) {
          error_field_name = String::Format("adComponents[%u].sizeGroup", i);
          error_field_value = ad_component_size_group;
          error = "Size group name cannot be empty.";
          return false;
        }
        if (!group.size_groups ||
            !group.size_groups->Contains(ad_component_size_group)) {
          error_field_name = String::Format("adComponents[%u].sizeGroup", i);
          error_field_value = ad_component_size_group;
          error = "The assigned size group does not exist in sizeGroups map.";
          return false;
        }
      }
      if (group.ad_components.value()[i]->ad_render_id.length() >
          kMaxAdRenderIdSize) {
        error_field_name = String::Format("adComponents[%u].adRenderId", i);
        error_field_value = group.ad_components.value()[i]->ad_render_id;
        error = "The adRenderId is too long.";
        return false;
      }

      // The code should not be setting these for `ad_components`
      DCHECK(group.ad_components.value()[i]->buyer_reporting_id.IsNull());
      DCHECK(group.ad_components.value()[i]
                 ->buyer_and_seller_reporting_id.IsNull());
      DCHECK(!group.ad_components.value()[i]
                  ->selectable_buyer_and_seller_reporting_ids.has_value());
      DCHECK(!group.ad_components.value()[i]
                  ->allowed_reporting_origins.has_value());
    }
  }

  if (group.ad_sizes) {
    for (auto const& it : group.ad_sizes.value()) {
      if (it.key == "") {
        error_field_name = "adSizes";
        error_field_value = it.key;
        error = "Ad sizes cannot map from an empty event name.";
        return false;
      }
      if (it.value->width_units == mojom::blink::AdSize::LengthUnit::kInvalid ||
          it.value->height_units ==
              mojom::blink::AdSize::LengthUnit::kInvalid) {
        error_field_name = "adSizes";
        error_field_value = "";
        error =
            "Ad size dimensions must be a valid number either in pixels (px) "
            "or screen width (sw).";
        return false;
      }
      if (it.value->width <= 0 || it.value->height <= 0 ||
          !std::isfinite(it.value->width) || !std::isfinite(it.value->height)) {
        error_field_name = "adSizes";
        error_field_value =
            String::Format("%f x %f", it.value->width, it.value->height);
        error =
            "Ad sizes must have a valid (non-zero/non-infinite) width and "
            "height.";
        return false;
      }
    }
  }

  if (group.size_groups) {
    if (!group.ad_sizes) {
      error_field_name = "sizeGroups";
      error_field_value = "";
      error = "An adSizes map must exist for sizeGroups to work.";
      return false;
    }
    for (auto const& [group_name, sizes] : group.size_groups.value()) {
      if (group_name == "") {
        error_field_name = "sizeGroups";
        error_field_value = group_name;
        error = "Size groups cannot map from an empty group name.";
        return false;
      }
      for (auto const& size : sizes) {
        if (size == "") {
          error_field_name = "sizeGroups";
          error_field_value = size;
          error = "Size groups cannot map to an empty ad size name.";
          return false;
        }
        if (!group.ad_sizes->Contains(size)) {
          error_field_name = "sizeGroups";
          error_field_value = size;
          error = "Size does not exist in adSizes map.";
          return false;
        }
      }
    }
  }

  if (group.additional_bid_key) {
    if (group.additional_bid_key->size() != X25519_PUBLIC_VALUE_LEN) {
      error_field_name = "additionalBidKey";
      error_field_value = String::Number(group.additional_bid_key->size());
      error = String::Format("additionalBidKey must be exactly %u bytes.",
                             X25519_PUBLIC_VALUE_LEN);
      return false;
    }
  }

  if (group.additional_bid_key && group.ads) {
    error =
        "Interest groups that provide a value of additionalBidKey "
        "for negative targeting must not provide a value for ads.";
    return false;
  }

  if (group.additional_bid_key && group.update_url) {
    error =
        "Interest groups that provide a value of additionalBidKey "
        "for negative targeting must not provide an updateURL.";
    return false;
  }

  if (group.aggregation_coordinator_origin &&
      group.aggregation_coordinator_origin->Protocol() != url::kHttpsScheme) {
    error_field_name = "aggregationCoordinatorOrigin";
    error_field_value = group.aggregation_coordinator_origin->ToString();
    error = "aggregationCoordinatorOrigin origin must be HTTPS.";
    return false;
  }

  size_t size = EstimateBlinkInterestGroupSize(group);
  if (size >= mojom::blink::kMaxInterestGroupSize) {
    error_field_name = "size";
    error_field_value = String::Number(size);
    error = String::Format("interest groups must be less than %u bytes",
                           mojom::blink::kMaxInterestGroupSize);
    return false;
  }

  return true;
}

}  // namespace blink
