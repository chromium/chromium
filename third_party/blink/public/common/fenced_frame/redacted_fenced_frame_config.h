// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See `content/browser/fenced_frame/fenced_frame_config.h` for a description of
// the fenced frame config information flow, including redacted configs.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_

#include <string>
#include <utility>
#include <vector>

#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct FencedFrameConfig;
struct FencedFrameProperties;
}  // namespace content

namespace blink::FencedFrame {

// This is used to represent the "opaque" union variant of "PotentiallyOpaque"
// mojom types.
enum class Opaque {
  kOpaque,
};

enum ReportingDestination {
  kBuyer,
  kSeller,
  kComponentSeller,
  kSharedStorageSelectUrl,
  kDirectSeller,
};

// TODO(crbug.com/1347953): Decompose this into flags that directly control the
// behavior of the frame, e.g. sandbox flags. We do not want mode to exist as a
// concept going forward.
enum DeprecatedFencedFrameMode {
  kDefault,
  kOpaqueAds,
};

struct BLINK_COMMON_EXPORT AdAuctionData {
  url::Origin interest_group_owner;
  std::string interest_group_name;
};

// The metadata for the shared storage runURLSelectionOperation's budget,
// which includes the shared storage's `site` and the amount of budget to
// charge when a fenced frame that originates from the URN is navigating a top
// frame. Before the fenced frame results in a top navigation, this
// `SharedStorageBudgetMetadata` will be stored/associated with the URN inside
// the `FencedFrameURLMapping`.
struct BLINK_COMMON_EXPORT SharedStorageBudgetMetadata {
  net::SchemefulSite site;
  double budget_to_charge = 0;

  // The bool `top_navigated` needs to be mutable because the overall
  // `FencedFrameConfig`/`FencedFrameProperties` object is const in virtually
  // all cases, except that we want to change this bool to true after a frame
  // with this config navigates the top for the first time.
  mutable bool top_navigated = false;
};

// Represents a potentially opaque (redacted) value.
// (If the value is redacted, `potentially_opaque_value` will be
// `absl::nullopt`.)
template <class T>
struct BLINK_COMMON_EXPORT RedactedFencedFrameProperty {
 public:
  RedactedFencedFrameProperty() : potentially_opaque_value(absl::nullopt) {}
  explicit RedactedFencedFrameProperty(
      const absl::optional<T>& potentially_opaque_value)
      : potentially_opaque_value(potentially_opaque_value) {}
  ~RedactedFencedFrameProperty() = default;

  absl::optional<T> potentially_opaque_value;
};

// Represents a fenced frame config that has been redacted for a particular
// entity, in order to send (over mojom) to the renderer corresponding to that
// entity.
// This object should only be constructed using
// `content::FencedFrameConfig::RedactFor(entity)`, or implicitly during
// mojom deserialization with the defined type mappings.
struct BLINK_COMMON_EXPORT RedactedFencedFrameConfig {
  RedactedFencedFrameConfig();
  ~RedactedFencedFrameConfig();

  const absl::optional<GURL>& urn_uuid() const { return urn_uuid_; }
  const absl::optional<RedactedFencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }
  const absl::optional<RedactedFencedFrameProperty<gfx::Size>>& container_size()
      const {
    return container_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<gfx::Size>>& content_size()
      const {
    return content_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<bool>>&
  deprecated_should_freeze_initial_size() const {
    return deprecated_should_freeze_initial_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<AdAuctionData>>&
  ad_auction_data() const {
    return ad_auction_data_;
  }
  const absl::optional<
      RedactedFencedFrameProperty<std::vector<RedactedFencedFrameConfig>>>&
  nested_configs() const {
    return nested_configs_;
  }
  const absl::optional<
      RedactedFencedFrameProperty<SharedStorageBudgetMetadata>>&
  shared_storage_budget_metadata() const {
    return shared_storage_budget_metadata_;
  }
  const DeprecatedFencedFrameMode& mode() const { return mode_; }
  const std::vector<blink::mojom::PermissionsPolicyFeature>&
  effective_enabled_permissions() const {
    return effective_enabled_permissions_;
  }

 private:
  friend struct content::FencedFrameConfig;
  friend struct mojo::StructTraits<
      blink::mojom::FencedFrameConfigDataView,
      blink::FencedFrame::RedactedFencedFrameConfig>;

  absl::optional<GURL> urn_uuid_;
  absl::optional<RedactedFencedFrameProperty<GURL>> mapped_url_;
  absl::optional<RedactedFencedFrameProperty<gfx::Size>> container_size_;
  absl::optional<RedactedFencedFrameProperty<gfx::Size>> content_size_;
  absl::optional<RedactedFencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;
  absl::optional<RedactedFencedFrameProperty<AdAuctionData>> ad_auction_data_;
  absl::optional<
      RedactedFencedFrameProperty<std::vector<RedactedFencedFrameConfig>>>
      nested_configs_;
  absl::optional<RedactedFencedFrameProperty<SharedStorageBudgetMetadata>>
      shared_storage_budget_metadata_;

  // TODO(crbug.com/1347953): Not yet used.
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;
};

// Represents a set of fenced frame properties (instantiated from a config) that
// have been redacted for a particular entity, in order to send (over mojom) to
// the renderer corresponding to that entity.
// This object should only be constructed using
// `content::FencedFrameProperties::RedactFor(entity)`, or implicitly during
// mojom deserialization with the defined type mappings.
struct BLINK_COMMON_EXPORT RedactedFencedFrameProperties {
  RedactedFencedFrameProperties();
  ~RedactedFencedFrameProperties();

  const absl::optional<RedactedFencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }
  const absl::optional<RedactedFencedFrameProperty<gfx::Size>>& container_size()
      const {
    return container_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<gfx::Size>>& content_size()
      const {
    return content_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<bool>>&
  deprecated_should_freeze_initial_size() const {
    return deprecated_should_freeze_initial_size_;
  }
  const absl::optional<RedactedFencedFrameProperty<AdAuctionData>>&
  ad_auction_data() const {
    return ad_auction_data_;
  }
  const absl::optional<RedactedFencedFrameProperty<
      std::vector<std::pair<GURL, RedactedFencedFrameConfig>>>>&
  nested_urn_config_pairs() const {
    return nested_urn_config_pairs_;
  }
  const absl::optional<
      RedactedFencedFrameProperty<SharedStorageBudgetMetadata>>&
  shared_storage_budget_metadata() const {
    return shared_storage_budget_metadata_;
  }
  bool has_fenced_frame_reporting() const {
    return has_fenced_frame_reporting_;
  }
  const DeprecatedFencedFrameMode& mode() const { return mode_; }
  const std::vector<blink::mojom::PermissionsPolicyFeature>&
  effective_enabled_permissions() const {
    return effective_enabled_permissions_;
  }

 private:
  friend struct content::FencedFrameProperties;
  friend struct mojo::StructTraits<
      blink::mojom::FencedFramePropertiesDataView,
      blink::FencedFrame::RedactedFencedFrameProperties>;

  absl::optional<RedactedFencedFrameProperty<GURL>> mapped_url_;
  absl::optional<RedactedFencedFrameProperty<gfx::Size>> container_size_;
  absl::optional<RedactedFencedFrameProperty<gfx::Size>> content_size_;
  absl::optional<RedactedFencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;
  absl::optional<RedactedFencedFrameProperty<AdAuctionData>> ad_auction_data_;
  absl::optional<RedactedFencedFrameProperty<
      std::vector<std::pair<GURL, RedactedFencedFrameConfig>>>>
      nested_urn_config_pairs_;
  absl::optional<RedactedFencedFrameProperty<SharedStorageBudgetMetadata>>
      shared_storage_budget_metadata_;
  bool has_fenced_frame_reporting_ = false;
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;
  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;
};

}  // namespace blink::FencedFrame

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_
