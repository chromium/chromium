// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/validate_blink_interest_group.h"

#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "url/url_constants.h"

namespace blink {

namespace {

// Check if `url` can be used as an interest group's ad render URL. Ad URLs can
// be cross origin, unlike other interest group URLs, but are still restricted
// to HTTPS with no embedded credentials.
bool IsUrlAllowedForRenderUrls(const KURL& url) {
  if (!url.IsValid() || !url.ProtocolIs(url::kHttpsScheme))
    return false;

  return url.User().empty() && url.Pass().empty();
}

// Check if `url` can be used with the specified interest group for any of
// script URL, update URL, or realtime data URL. Ad render URLs should be
// checked with IsUrlAllowedForRenderUrls(), which doesn't have the same-origin
// check, and allows references.
bool IsUrlAllowed(const KURL& url, const mojom::blink::InterestGroup& group) {
  if (!group.owner->IsSameOriginWith(SecurityOrigin::Create(url).get()))
    return false;

  return IsUrlAllowedForRenderUrls(url) && !url.HasFragmentIdentifier();
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

  if (group.priority_vector)
    size += EstimateHashMapSize(*group.priority_vector);
  if (group.priority_signals_overrides)
    size += EstimateHashMapSize(*group.priority_signals_overrides);
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
  if (group.bidding_url)
    size += group.bidding_url->GetString().length();

  if (group.bidding_wasm_helper_url)
    size += group.bidding_wasm_helper_url->GetString().length();

  if (group.daily_update_url)
    size += group.daily_update_url->GetString().length();

  if (group.trusted_bidding_signals_url)
    size += group.trusted_bidding_signals_url->GetString().length();

  if (group.trusted_bidding_signals_keys) {
    for (const String& key : *group.trusted_bidding_signals_keys)
      size += key.length();
  }
  size += group.user_bidding_signals.length();

  if (group.ads) {
    for (const auto& ad : group.ads.value()) {
      size += ad->render_url.GetString().length();
      size += ad->metadata.length();
    }
  }

  if (group.ad_components) {
    for (const auto& ad : group.ad_components.value()) {
      size += ad->render_url.GetString().length();
      size += ad->metadata.length();
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

  if (!std::isfinite(group.priority)) {
    error_field_name = "priority";
    error_field_value = String::NumberToStringECMAScript(group.priority);
    error = "priority must be finite.";
    return false;
  }

  // This check is here to keep it in sync with InterestGroup::IsValid(), but
  // checks in navigator_auction.cc should ensure the execution mode is always
  // valid.
  if (group.execution_mode !=
          mojom::blink::InterestGroup::ExecutionMode::kCompatibilityMode &&
      group.execution_mode !=
          mojom::blink::InterestGroup::ExecutionMode::kGroupedByOriginMode) {
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
      error_field_name = "biddingUrl";
      error_field_value = group.bidding_url->GetString();
      error =
          "biddingUrl must have the same origin as the InterestGroup owner "
          "and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.bidding_wasm_helper_url) {
    if (!IsUrlAllowed(*group.bidding_wasm_helper_url, group)) {
      error_field_name = "biddingWasmHelperUrl";
      error_field_value = group.bidding_wasm_helper_url->GetString();
      error =
          "biddingWasmHelperUrl must have the same origin as the InterestGroup "
          "owner and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.daily_update_url) {
    if (!IsUrlAllowed(*group.daily_update_url, group)) {
      error_field_name = "updateUrl";
      error_field_value = group.daily_update_url->GetString();
      error =
          "updateUrl must have the same origin as the InterestGroup owner "
          "and have no fragment identifier or embedded credentials.";
      return false;
    }
  }

  if (group.trusted_bidding_signals_url) {
    // In addition to passing the same checks used on the other URLs,
    // `trusted_bidding_signals_url` must not have a query string, since the
    // query parameter needs to be set as part of running an auction.
    if (!IsUrlAllowed(*group.trusted_bidding_signals_url, group) ||
        !group.trusted_bidding_signals_url->Query().empty()) {
      error_field_name = "trustedBiddingSignalsUrl";
      error_field_value = group.trusted_bidding_signals_url->GetString();
      error =
          "trustedBiddingSignalsUrl must have the same origin as the "
          "InterestGroup owner and have no query string, fragment identifier "
          "or embedded credentials.";
      return false;
    }
  }

  if (group.ads) {
    for (WTF::wtf_size_t i = 0; i < group.ads.value().size(); ++i) {
      const KURL& render_url = group.ads.value()[i]->render_url;
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("ad[%u].renderUrl", i);
        error_field_value = render_url.GetString();
        error = "renderUrls must be HTTPS and have no embedded credentials.";
        return false;
      }
    }
  }

  if (group.ad_components) {
    for (WTF::wtf_size_t i = 0; i < group.ad_components.value().size(); ++i) {
      const KURL& render_url = group.ad_components.value()[i]->render_url;
      if (!IsUrlAllowedForRenderUrls(render_url)) {
        error_field_name = String::Format("adComponent[%u].renderUrl", i);
        error_field_value = render_url.GetString();
        error = "renderUrls must be HTTPS and have no embedded credentials.";
        return false;
      }
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
      if (it.value->width_units ==
              mojom::blink::InterestGroupSize::LengthUnit::kInvalid ||
          it.value->height_units ==
              mojom::blink::InterestGroupSize::LengthUnit::kInvalid) {
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
