// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_config.h"

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "third_party/blink/public/common/frame/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"

namespace content {

const char kUrnUuidPrefix[] = "urn:uuid:";

GURL GenerateUrnUuid() {
  return GURL(kUrnUuidPrefix +
              base::Uuid::GenerateRandomV4().AsLowercaseString());
}

std::string SubstituteMappedStrings(
    const std::string& input,
    const std::vector<std::pair<std::string, std::string>>& substitutions) {
  std::vector<std::string> output_vec;
  size_t input_idx = 0;
  while (input_idx < input.size()) {
    size_t replace_idx = input.size();
    size_t replace_end_idx = input.size();
    std::pair<std::string, std::string> const* next_replacement = nullptr;
    for (const auto& substitution : substitutions) {
      size_t found_idx = input.find(substitution.first, input_idx);
      if (found_idx < replace_idx) {
        replace_idx = found_idx;
        replace_end_idx = found_idx + substitution.first.size();
        next_replacement = &substitution;
      }
    }
    output_vec.push_back(input.substr(input_idx, replace_idx - input_idx));
    if (replace_idx < input.size()) {
      output_vec.push_back(next_replacement->second);
    }
    // move input index to after what we replaced (or end of string).
    input_idx = replace_end_idx;
  }
  return base::StrCat(output_vec);
}

namespace {

template <typename Property>
void RedactProperty(
    const std::optional<FencedFrameProperty<Property>>& property,
    FencedFrameEntity entity,
    std::optional<blink::FencedFrame::RedactedFencedFrameProperty<Property>>&
        out) {
  if (property.has_value()) {
    out = blink::FencedFrame::RedactedFencedFrameProperty(
        property->GetValueForEntity(entity));
  }
}

}  // namespace

FencedFrameConfig::FencedFrameConfig() = default;

FencedFrameConfig::FencedFrameConfig(const GURL& mapped_url)
    : mapped_url_(std::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      mode_(DeprecatedFencedFrameMode::kOpaqueAds) {}

FencedFrameConfig::FencedFrameConfig(
    const GURL& mapped_url,
    const gfx::Size& content_size,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter,
    bool is_ad_component)
    : mapped_url_(std::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      content_size_(std::in_place,
                    content_size,
                    VisibilityToEmbedder::kTransparent,
                    VisibilityToContent::kTransparent),
      deprecated_should_freeze_initial_size_(std::in_place,
                                             false,
                                             VisibilityToEmbedder::kTransparent,
                                             VisibilityToContent::kOpaque),
      fenced_frame_reporter_(fenced_frame_reporter),
      is_ad_component_(is_ad_component) {}

FencedFrameConfig::FencedFrameConfig(const GURL& urn_uuid,
                                     const GURL& mapped_url)
    : urn_uuid_(urn_uuid),
      mapped_url_(std::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      mode_(DeprecatedFencedFrameMode::kOpaqueAds) {}

FencedFrameConfig::FencedFrameConfig(
    const GURL& mapped_url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter,
    bool is_ad_component)
    : FencedFrameConfig(mapped_url) {
  fenced_frame_reporter_ = fenced_frame_reporter;
  is_ad_component_ = is_ad_component;
}

FencedFrameConfig::FencedFrameConfig(
    const GURL& urn_uuid,
    const GURL& mapped_url,
    const SharedStorageBudgetMetadata& shared_storage_budget_metadata,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter)
    : urn_uuid_(urn_uuid),
      mapped_url_(std::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      deprecated_should_freeze_initial_size_(std::in_place,
                                             false,
                                             VisibilityToEmbedder::kTransparent,
                                             VisibilityToContent::kOpaque),
      shared_storage_budget_metadata_(std::in_place,
                                      shared_storage_budget_metadata,
                                      VisibilityToEmbedder::kOpaque,
                                      VisibilityToContent::kOpaque),
      fenced_frame_reporter_(std::move(fenced_frame_reporter)),
      mode_(DeprecatedFencedFrameMode::kOpaqueAds) {}

FencedFrameConfig::FencedFrameConfig(const FencedFrameConfig&) = default;
FencedFrameConfig::FencedFrameConfig(FencedFrameConfig&&) = default;
FencedFrameConfig::~FencedFrameConfig() = default;

FencedFrameConfig& FencedFrameConfig::operator=(const FencedFrameConfig&) =
    default;
FencedFrameConfig& FencedFrameConfig::operator=(FencedFrameConfig&&) = default;

blink::FencedFrame::RedactedFencedFrameConfig FencedFrameConfig::RedactFor(
    FencedFrameEntity entity) const {
  blink::FencedFrame::RedactedFencedFrameConfig redacted_config;
  if (urn_uuid_.has_value()) {
    redacted_config.urn_uuid_ = urn_uuid_;
  }

  RedactProperty(mapped_url_, entity, redacted_config.mapped_url_);
  RedactProperty(container_size_, entity, redacted_config.container_size_);
  RedactProperty(content_size_, entity, redacted_config.content_size_);
  RedactProperty(deprecated_should_freeze_initial_size_, entity,
                 redacted_config.deprecated_should_freeze_initial_size_);
  RedactProperty(ad_auction_data_, entity, redacted_config.ad_auction_data_);

  if (nested_configs_.has_value()) {
    std::optional<std::vector<FencedFrameConfig>>
        partially_redacted_nested_configs =
            nested_configs_->GetValueForEntity(entity);
    if (partially_redacted_nested_configs.has_value()) {
      redacted_config.nested_configs_.emplace(
          std::vector<blink::FencedFrame::RedactedFencedFrameConfig>());
      for (const FencedFrameConfig& nested_config :
           partially_redacted_nested_configs.value()) {
        redacted_config.nested_configs_->potentially_opaque_value->emplace_back(
            nested_config.RedactFor(FencedFrameEntity::kEmbedder));
      }
    } else {
      redacted_config.nested_configs_.emplace(std::nullopt);
    }
  }

  RedactProperty(shared_storage_budget_metadata_, entity,
                 redacted_config.shared_storage_budget_metadata_);

  // The mode never needs to be redacted, because it is a function of which API
  // was called to generate the config, rather than any cross-site data.
  redacted_config.mode_ = mode_;

  redacted_config.effective_enabled_permissions_ =
      effective_enabled_permissions_;

  redacted_config.parent_permissions_info_ = parent_permissions_info_;

  return redacted_config;
}

FencedFrameProperties::FencedFrameProperties()
    : ad_auction_data_(std::nullopt),
      nested_urn_config_pairs_(std::nullopt),
      shared_storage_budget_metadata_(std::nullopt),
      embedder_shared_storage_context_(std::nullopt),
      partition_nonce_(std::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque) {}

FencedFrameProperties::FencedFrameProperties(const GURL& mapped_url)
    : mapped_url_(std::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kTransparent,
                  VisibilityToContent::kTransparent),
      partition_nonce_(std::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque),
      allows_information_inflow_(true) {}

FencedFrameProperties::FencedFrameProperties(const FencedFrameConfig& config)
    : mapped_url_(config.mapped_url_),
      container_size_(config.container_size_),
      content_size_(config.content_size_),
      deprecated_should_freeze_initial_size_(
          config.deprecated_should_freeze_initial_size_),
      ad_auction_data_(config.ad_auction_data_),
      on_navigate_callback_(config.on_navigate_callback_),
      nested_urn_config_pairs_(std::nullopt),
      shared_storage_budget_metadata_(std::nullopt),
      embedder_shared_storage_context_(std::nullopt),
      fenced_frame_reporter_(config.fenced_frame_reporter_),
      partition_nonce_(std::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque),
      mode_(config.mode_),
      allows_information_inflow_(config.allows_information_inflow_),
      is_ad_component_(config.is_ad_component_),
      effective_enabled_permissions_(config.effective_enabled_permissions_),
      parent_permissions_info_(config.parent_permissions_info_) {
  if (config.shared_storage_budget_metadata_) {
    shared_storage_budget_metadata_.emplace(
        &config.shared_storage_budget_metadata_->GetValueIgnoringVisibility(),
        config.shared_storage_budget_metadata_->visibility_to_embedder_,
        config.shared_storage_budget_metadata_->visibility_to_content_);
  }
  if (config.nested_configs_) {
    nested_urn_config_pairs_.emplace(
        GenerateURNConfigVectorForConfigs(
            config.nested_configs_->GetValueIgnoringVisibility()),
        config.nested_configs_->visibility_to_embedder_,
        config.nested_configs_->visibility_to_content_);
  }
}

FencedFrameProperties::FencedFrameProperties(const FencedFrameProperties&) =
    default;
FencedFrameProperties::FencedFrameProperties(FencedFrameProperties&&) = default;
FencedFrameProperties::~FencedFrameProperties() = default;

FencedFrameProperties& FencedFrameProperties::operator=(
    const FencedFrameProperties&) = default;
FencedFrameProperties& FencedFrameProperties::operator=(
    FencedFrameProperties&&) = default;

blink::FencedFrame::RedactedFencedFrameProperties
FencedFrameProperties::RedactFor(FencedFrameEntity entity) const {
  blink::FencedFrame::RedactedFencedFrameProperties redacted_properties;
  RedactProperty(mapped_url_, entity, redacted_properties.mapped_url_);
  RedactProperty(container_size_, entity, redacted_properties.container_size_);
  RedactProperty(content_size_, entity, redacted_properties.content_size_);
  RedactProperty(deprecated_should_freeze_initial_size_, entity,
                 redacted_properties.deprecated_should_freeze_initial_size_);
  RedactProperty(ad_auction_data_, entity,
                 redacted_properties.ad_auction_data_);

  if (nested_urn_config_pairs_.has_value()) {
    std::optional<std::vector<std::pair<GURL, FencedFrameConfig>>>
        partially_redacted_nested_urn_config_pairs =
            nested_urn_config_pairs_->GetValueForEntity(entity);
    if (partially_redacted_nested_urn_config_pairs.has_value()) {
      redacted_properties.nested_urn_config_pairs_.emplace(
          std::vector<std::pair<
              GURL, blink::FencedFrame::RedactedFencedFrameConfig>>());
      for (const std::pair<GURL, FencedFrameConfig>& nested_urn_config_pair :
           *partially_redacted_nested_urn_config_pairs) {
        redacted_properties.nested_urn_config_pairs_->potentially_opaque_value
            ->emplace_back(nested_urn_config_pair.first,
                           nested_urn_config_pair.second.RedactFor(
                               FencedFrameEntity::kEmbedder));
      }
    } else {
      redacted_properties.nested_urn_config_pairs_.emplace(std::nullopt);
    }
  }
  if (shared_storage_budget_metadata_.has_value()) {
    std::optional<raw_ptr<const SharedStorageBudgetMetadata>>
        potentially_opaque_ptr =
            shared_storage_budget_metadata_->GetValueForEntity(entity);
    if (potentially_opaque_ptr.has_value()) {
      redacted_properties.shared_storage_budget_metadata_ =
          blink::FencedFrame::RedactedFencedFrameProperty(
              std::make_optional(*potentially_opaque_ptr.value()));
    } else {
      redacted_properties.shared_storage_budget_metadata_.emplace(std::nullopt);
    }
  }

  if (fenced_frame_reporter_ || is_ad_component_) {
    // An ad component should use its parent's fenced frame reporter. Even
    // though it does not have a reporter in its `FencedFrameProperties`, this
    // flag is still marked as true. Content that is cross-origin to the
    // config's mapped url gets access to its parent's reporter only if both the
    // parent and the content opt in to cross-origin event reporting.
    redacted_properties.has_fenced_frame_reporting_ = true;
  }

  // The mode never needs to be redacted, because it is a function of which API
  // was called to generate the config, rather than any cross-site data.
  redacted_properties.mode_ = mode_;

  redacted_properties.effective_enabled_permissions_ =
      effective_enabled_permissions_;

  redacted_properties.parent_permissions_info_ = parent_permissions_info_;

  if (entity != FencedFrameEntity::kCrossOriginContent) {
    redacted_properties.can_disable_untrusted_network_ =
        can_disable_untrusted_network_;
  }

  redacted_properties.is_cross_origin_content_ =
      entity == FencedFrameEntity::kCrossOriginContent;
  redacted_properties.allow_cross_origin_event_reporting_ =
      allow_cross_origin_event_reporting_;

  return redacted_properties;
}

void FencedFrameProperties::UpdateMappedURL(GURL url) {
  CHECK(mapped_url_.has_value());
  mapped_url_->value_ = url;
}

std::vector<std::pair<GURL, FencedFrameConfig>>
FencedFrameProperties::GenerateURNConfigVectorForConfigs(
    const std::vector<FencedFrameConfig>& nested_configs) {
  std::vector<std::pair<GURL, FencedFrameConfig>> nested_urn_config_pairs;
  const size_t kMaxAdAuctionAdComponents = blink::MaxAdAuctionAdComponents();
  DCHECK_LE(nested_configs.size(), kMaxAdAuctionAdComponents);
  for (const FencedFrameConfig& config : nested_configs) {
    // Give each config its own urn:uuid. This ensures that if the same config
    // is loaded into multiple fenced frames, they will not share the same
    // urn:uuid across processes.
    GURL urn_uuid = GenerateUrnUuid();
    FencedFrameConfig config_with_urn = config;
    config_with_urn.urn_uuid_ = urn_uuid;
    nested_urn_config_pairs.emplace_back(urn_uuid, config_with_urn);
  }

  // Pad `component_ads_` to contain exactly MaxAdAuctionAdComponents() ads, to
  // avoid leaking any data to the fenced frame the component ads array is
  // exposed to.
  while (nested_urn_config_pairs.size() < kMaxAdAuctionAdComponents) {
    GURL urn_uuid = GenerateUrnUuid();
    nested_urn_config_pairs.emplace_back(
        urn_uuid, FencedFrameConfig(urn_uuid, GURL(url::kAboutBlankURL)));
  }
  return nested_urn_config_pairs;
}

void FencedFrameProperties::UpdateParentParsedPermissionsPolicy(
    const blink::PermissionsPolicy* parent_policy,
    const url::Origin& parent_origin) {
  // Sanity check that a fenced frame loaded through Protected Audience or
  // Shared Storage did not reach this point. `effective_enabled_permissions_`
  // is populated in `fenced_frame_url_mapping.cc` if loaded through an API. If
  // loaded through any other means, the vector remains empty.
  CHECK_EQ(effective_enabled_permissions_.size(), 0u);
  CHECK(parent_policy);
  std::vector<blink::ParsedPermissionsPolicyDeclaration> parsed_policies;
  for (auto feature : blink::kFencedFrameAllowedFeatures) {
    const blink::PermissionsPolicy::Allowlist allow_list =
        parent_policy->GetAllowlistForFeature(feature);
    parsed_policies.emplace_back(
        feature, allow_list.AllowedOrigins(), allow_list.SelfIfMatches(),
        allow_list.MatchesAll(), allow_list.MatchesOpaqueSrc());
  }
  parent_permissions_info_.emplace(parsed_policies, parent_origin);
}

}  // namespace content
