// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See `content/browser/fenced_frame/fenced_frame_config.h` for a description of
// the fenced frame config information flow, including redacted configs.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom-forward.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class FencedFrameConfig;
class FencedFrameProperties;
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

struct BLINK_COMMON_EXPORT ParentPermissionsInfo {
  std::vector<network::ParsedPermissionsPolicyDeclaration>
      parsed_permissions_policy;
  url::Origin origin;
};

// Represents a potentially opaque (redacted) value.
// (If the value is redacted, `potentially_opaque_value` will be
// `std::nullopt`.)
template <class T>
struct BLINK_COMMON_EXPORT RedactedFencedFrameProperty {
 public:
  RedactedFencedFrameProperty() : potentially_opaque_value(std::nullopt) {}
  explicit RedactedFencedFrameProperty(
      const std::optional<T>& potentially_opaque_value)
      : potentially_opaque_value(potentially_opaque_value) {}
  ~RedactedFencedFrameProperty() = default;

  std::optional<T> potentially_opaque_value;
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

  const std::optional<GURL>& urn_uuid() const { return urn_uuid_; }
  const std::optional<RedactedFencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }
  const std::optional<RedactedFencedFrameProperty<gfx::Size>>& container_size()
      const {
    return container_size_;
  }
  const std::optional<RedactedFencedFrameProperty<gfx::Size>>& content_size()
      const {
    return content_size_;
  }
  const std::optional<RedactedFencedFrameProperty<bool>>&
  deprecated_should_freeze_initial_size() const {
    return deprecated_should_freeze_initial_size_;
  }
  const std::optional<RedactedFencedFrameProperty<AdAuctionData>>&
  ad_auction_data() const {
    return ad_auction_data_;
  }
  const std::optional<
      RedactedFencedFrameProperty<std::vector<RedactedFencedFrameConfig>>>&
  nested_configs() const {
    return nested_configs_;
  }
  const DeprecatedFencedFrameMode& mode() const { return mode_; }
  const std::vector<network::mojom::PermissionsPolicyFeature>&
  effective_enabled_permissions() const {
    return effective_enabled_permissions_;
  }
  const std::optional<ParentPermissionsInfo> parent_permissions_info() const {
    return parent_permissions_info_;
  }

 private:
  friend class content::FencedFrameConfig;
  friend struct mojo::StructTraits<
      blink::mojom::FencedFrameConfigDataView,
      blink::FencedFrame::RedactedFencedFrameConfig>;

  std::optional<GURL> urn_uuid_;
  std::optional<RedactedFencedFrameProperty<GURL>> mapped_url_;
  std::optional<RedactedFencedFrameProperty<gfx::Size>> container_size_;
  std::optional<RedactedFencedFrameProperty<gfx::Size>> content_size_;
  std::optional<RedactedFencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;
  std::optional<RedactedFencedFrameProperty<AdAuctionData>> ad_auction_data_;
  std::optional<
      RedactedFencedFrameProperty<std::vector<RedactedFencedFrameConfig>>>
      nested_configs_;

  // TODO(crbug.com/1347953): Not yet used.
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  std::vector<network::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;

  // Fenced frames with flexible permissions are allowed to inherit certain
  // permissions policies from their parent. However, a fenced frame's renderer
  // process doesn't have access to its parent. Since this is how
  // `SecurityContextInit::ApplyPermissionsPolicy()` learns what the parent
  // permissions policies are, this will not work for MPArch. Instead, the
  // browser gives the renderer this information through the fenced frame
  // config.
  std::optional<ParentPermissionsInfo> parent_permissions_info_;
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

  const std::optional<RedactedFencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }
  const std::optional<RedactedFencedFrameProperty<gfx::Size>>& container_size()
      const {
    return container_size_;
  }
  const std::optional<RedactedFencedFrameProperty<gfx::Size>>& content_size()
      const {
    return content_size_;
  }
  const std::optional<RedactedFencedFrameProperty<bool>>&
  deprecated_should_freeze_initial_size() const {
    return deprecated_should_freeze_initial_size_;
  }
  const std::optional<RedactedFencedFrameProperty<AdAuctionData>>&
  ad_auction_data() const {
    return ad_auction_data_;
  }
  const std::optional<RedactedFencedFrameProperty<
      std::vector<std::pair<GURL, RedactedFencedFrameConfig>>>>&
  nested_urn_config_pairs() const {
    return nested_urn_config_pairs_;
  }
  const DeprecatedFencedFrameMode& mode() const { return mode_; }
  const std::vector<network::mojom::PermissionsPolicyFeature>&
  effective_enabled_permissions() const {
    return effective_enabled_permissions_;
  }
  const std::optional<ParentPermissionsInfo> parent_permissions_info() const {
    return parent_permissions_info_;
  }
  bool is_cross_origin_content() const { return is_cross_origin_content_; }
  bool allow_cross_origin_event_reporting() const {
    return allow_cross_origin_event_reporting_;
  }

 private:
  friend class content::FencedFrameProperties;
  friend struct mojo::StructTraits<
      blink::mojom::FencedFramePropertiesDataView,
      blink::FencedFrame::RedactedFencedFrameProperties>;

  std::optional<RedactedFencedFrameProperty<GURL>> mapped_url_;
  std::optional<RedactedFencedFrameProperty<gfx::Size>> container_size_;
  std::optional<RedactedFencedFrameProperty<gfx::Size>> content_size_;
  std::optional<RedactedFencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;
  std::optional<RedactedFencedFrameProperty<AdAuctionData>> ad_auction_data_;
  std::optional<RedactedFencedFrameProperty<
      std::vector<std::pair<GURL, RedactedFencedFrameConfig>>>>
      nested_urn_config_pairs_;
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;
  std::vector<network::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;
  std::optional<ParentPermissionsInfo> parent_permissions_info_;
  bool is_cross_origin_content_ = false;
  bool allow_cross_origin_event_reporting_ = false;
};

}  // namespace blink::FencedFrame

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FENCED_FRAME_REDACTED_FENCED_FRAME_CONFIG_H_
