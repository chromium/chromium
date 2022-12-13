// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"

namespace content {

const char kUrnUuidPrefix[] = "urn:uuid:";

GURL GenerateUrnUuid() {
  return GURL(kUrnUuidPrefix +
              base::GUID::GenerateRandomV4().AsLowercaseString());
}

namespace {

std::vector<std::pair<GURL, FencedFrameConfig>>
GenerateURNConfigVectorForConfigs(
    const std::vector<FencedFrameConfig>& nested_configs) {
  std::vector<std::pair<GURL, FencedFrameConfig>> nested_urn_config_pairs;
  DCHECK_LE(nested_configs.size(), blink::kMaxAdAuctionAdComponents);
  for (const FencedFrameConfig& config : nested_configs) {
    // Give each config its own urn:uuid. This ensures that if the same config
    // is loaded into multiple fenced frames, they will not share the same
    // urn:uuid across processes.
    GURL urn_uuid = GenerateUrnUuid();
    auto config_with_urn = config;
    config_with_urn.urn_ = urn_uuid;
    nested_urn_config_pairs.emplace_back(urn_uuid, config_with_urn);
  }

  // Pad `component_ads_` to contain exactly kMaxAdAuctionAdComponents ads, to
  // avoid leaking any data to the fenced frame the component ads array is
  // exposed to.
  while (nested_urn_config_pairs.size() < blink::kMaxAdAuctionAdComponents) {
    GURL urn_uuid = GenerateUrnUuid();
    nested_urn_config_pairs.emplace_back(
        urn_uuid, FencedFrameConfig(urn_uuid, GURL(url::kAboutBlankURL)));
  }
  return nested_urn_config_pairs;
}

template <typename Property>
void RedactProperty(
    const absl::optional<FencedFrameProperty<Property>>& property,
    FencedFrameEntity entity,
    absl::optional<blink::FencedFrame::RedactedFencedFrameProperty<Property>>&
        out) {
  if (property.has_value()) {
    out = blink::FencedFrame::RedactedFencedFrameProperty(
        property->GetValueForEntity(entity));
  }
}

}  // namespace

FencedFrameConfig::FencedFrameConfig() = default;

FencedFrameConfig::FencedFrameConfig(const GURL& mapped_url)
    : mapped_url_(absl::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent) {}

FencedFrameConfig::FencedFrameConfig(GURL urn, const GURL& mapped_url)
    : urn_(urn),
      mapped_url_(absl::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent) {}

FencedFrameConfig::FencedFrameConfig(
    GURL urn,
    const GURL& mapped_url,
    const SharedStorageBudgetMetadata& shared_storage_budget_metadata,
    const ReportingMetadata& reporting_metadata)
    : urn_(urn),
      mapped_url_(absl::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      shared_storage_budget_metadata_(absl::in_place,
                                      shared_storage_budget_metadata,
                                      VisibilityToEmbedder::kOpaque,
                                      VisibilityToContent::kOpaque),
      // TODO(crbug.com/1381158): Give the reporting metadata
      // `VisibilityToContent::kOpaque` once it is no longer needed in the
      // renderer.
      reporting_metadata_(absl::in_place,
                          reporting_metadata,
                          VisibilityToEmbedder::kOpaque,
                          VisibilityToContent::kTransparent) {}

FencedFrameConfig::FencedFrameConfig(const FencedFrameConfig&) = default;
FencedFrameConfig::FencedFrameConfig(FencedFrameConfig&&) = default;
FencedFrameConfig::~FencedFrameConfig() = default;

FencedFrameConfig& FencedFrameConfig::operator=(const FencedFrameConfig&) =
    default;
FencedFrameConfig& FencedFrameConfig::operator=(FencedFrameConfig&&) = default;

blink::FencedFrame::RedactedFencedFrameConfig FencedFrameConfig::RedactFor(
    FencedFrameEntity entity) const {
  blink::FencedFrame::RedactedFencedFrameConfig redacted_config;
  if (urn_.has_value()) {
    redacted_config.urn_ = urn_;
  }

  RedactProperty(mapped_url_, entity, redacted_config.mapped_url_);
  RedactProperty(container_size_, entity, redacted_config.container_size_);
  RedactProperty(content_size_, entity, redacted_config.content_size_);
  RedactProperty(deprecated_should_freeze_initial_size_, entity,
                 redacted_config.deprecated_should_freeze_initial_size_);
  RedactProperty(ad_auction_data_, entity, redacted_config.ad_auction_data_);

  if (nested_configs_.has_value()) {
    absl::optional<std::vector<FencedFrameConfig>>
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
      redacted_config.nested_configs_.emplace(absl::nullopt);
    }
  }

  RedactProperty(shared_storage_budget_metadata_, entity,
                 redacted_config.shared_storage_budget_metadata_);
  RedactProperty(reporting_metadata_, entity,
                 redacted_config.reporting_metadata_);

  return redacted_config;
}

FencedFrameProperties::FencedFrameProperties()
    : ad_auction_data_(absl::nullopt),
      nested_urn_config_pairs_(absl::nullopt),
      shared_storage_budget_metadata_(absl::nullopt),
      partition_nonce_(absl::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque) {}

FencedFrameProperties::FencedFrameProperties(const FencedFrameConfig& config)
    : urn_(config.urn_),
      mapped_url_(config.mapped_url_),
      container_size_(config.container_size_),
      content_size_(config.content_size_),
      deprecated_should_freeze_initial_size_(
          config.deprecated_should_freeze_initial_size_),
      ad_auction_data_(config.ad_auction_data_),
      on_navigate_callback_(config.on_navigate_callback_),
      nested_urn_config_pairs_(absl::nullopt),
      shared_storage_budget_metadata_(absl::nullopt),
      reporting_metadata_(config.reporting_metadata_),
      partition_nonce_(absl::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque) {
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
  if (urn_.has_value()) {
    redacted_properties.urn_ = urn_;
  }

  RedactProperty(mapped_url_, entity, redacted_properties.mapped_url_);
  RedactProperty(container_size_, entity, redacted_properties.container_size_);
  RedactProperty(content_size_, entity, redacted_properties.content_size_);
  RedactProperty(deprecated_should_freeze_initial_size_, entity,
                 redacted_properties.deprecated_should_freeze_initial_size_);
  RedactProperty(ad_auction_data_, entity,
                 redacted_properties.ad_auction_data_);

  if (nested_urn_config_pairs_.has_value()) {
    absl::optional<std::vector<std::pair<GURL, FencedFrameConfig>>>
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
      redacted_properties.nested_urn_config_pairs_.emplace(absl::nullopt);
    }
  }
  if (shared_storage_budget_metadata_.has_value()) {
    absl::optional<raw_ptr<const SharedStorageBudgetMetadata>>
        potentially_opaque_ptr =
            shared_storage_budget_metadata_->GetValueForEntity(entity);
    if (potentially_opaque_ptr.has_value()) {
      redacted_properties.shared_storage_budget_metadata_ =
          blink::FencedFrame::RedactedFencedFrameProperty(
              absl::make_optional(*potentially_opaque_ptr.value()));
    } else {
      redacted_properties.shared_storage_budget_metadata_.emplace(
          absl::nullopt);
    }
  }

  RedactProperty(reporting_metadata_, entity,
                 redacted_properties.reporting_metadata_);

  return redacted_properties;
}

}  // namespace content
