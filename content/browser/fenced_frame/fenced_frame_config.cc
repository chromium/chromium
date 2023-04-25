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
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"

namespace content {

const char kUrnUuidPrefix[] = "urn:uuid:";

GURL GenerateUrnUuid() {
  return GURL(kUrnUuidPrefix +
              base::Uuid::GenerateRandomV4().AsLowercaseString());
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
    config_with_urn.urn_uuid_ = urn_uuid;
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
                  VisibilityToContent::kTransparent),
      mode_(DeprecatedFencedFrameMode::kOpaqueAds) {}

FencedFrameConfig::FencedFrameConfig(
    const GURL& mapped_url,
    const gfx::Size& content_size,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter,
    bool is_ad_component)
    : mapped_url_(absl::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      content_size_(absl::in_place,
                    content_size,
                    VisibilityToEmbedder::kTransparent,
                    VisibilityToContent::kTransparent),
      deprecated_should_freeze_initial_size_(absl::in_place,
                                             false,
                                             VisibilityToEmbedder::kTransparent,
                                             VisibilityToContent::kOpaque),
      fenced_frame_reporter_(fenced_frame_reporter),
      is_ad_component_(is_ad_component) {}

FencedFrameConfig::FencedFrameConfig(const GURL& urn_uuid,
                                     const GURL& mapped_url)
    : urn_uuid_(urn_uuid),
      mapped_url_(absl::in_place,
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
      mapped_url_(absl::in_place,
                  mapped_url,
                  VisibilityToEmbedder::kOpaque,
                  VisibilityToContent::kTransparent),
      deprecated_should_freeze_initial_size_(absl::in_place,
                                             false,
                                             VisibilityToEmbedder::kTransparent,
                                             VisibilityToContent::kOpaque),
      shared_storage_budget_metadata_(absl::in_place,
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

  // The mode never needs to be redacted, because it is a function of which API
  // was called to generate the config, rather than any cross-site data.
  redacted_config.mode_ = mode_;

  return redacted_config;
}

FencedFrameProperties::FencedFrameProperties()
    : ad_auction_data_(absl::nullopt),
      nested_urn_config_pairs_(absl::nullopt),
      shared_storage_budget_metadata_(absl::nullopt),
      embedder_shared_storage_context_(absl::nullopt),
      partition_nonce_(absl::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque) {}

FencedFrameProperties::FencedFrameProperties(const FencedFrameConfig& config)
    : mapped_url_(config.mapped_url_),
      container_size_(config.container_size_),
      content_size_(config.content_size_),
      deprecated_should_freeze_initial_size_(
          config.deprecated_should_freeze_initial_size_),
      ad_auction_data_(config.ad_auction_data_),
      on_navigate_callback_(config.on_navigate_callback_),
      nested_urn_config_pairs_(absl::nullopt),
      shared_storage_budget_metadata_(absl::nullopt),
      embedder_shared_storage_context_(absl::nullopt),
      fenced_frame_reporter_(config.fenced_frame_reporter_),
      partition_nonce_(absl::in_place,
                       base::UnguessableToken::Create(),
                       VisibilityToEmbedder::kOpaque,
                       VisibilityToContent::kOpaque),
      mode_(config.mode_),
      is_ad_component_(config.is_ad_component_) {
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

  if (fenced_frame_reporter_ || is_ad_component_) {
    // An ad component should use its parent's fenced frame reporter. Even
    // though it does not have a reporter in its `FencedFrameProperties`, this
    // flag is still marked as true.
    redacted_properties.has_fenced_frame_reporting_ = true;
  }

  // The mode never needs to be redacted, because it is a function of which API
  // was called to generate the config, rather than any cross-site data.
  redacted_properties.mode_ = mode_;

  return redacted_properties;
}

void FencedFrameProperties::UpdateMappedURL(GURL url) {
  CHECK(mapped_url_.has_value());
  mapped_url_->value_ = url;
}

void FencedFrameProperties::UpdateAutomaticBeaconData(
    const std::string& event_data,
    const std::vector<blink::FencedFrame::ReportingDestination>& destinations) {
  // For an ad component, the event data from its automatic beacon is ignored.
  automatic_beacon_info_.emplace(is_ad_component_ ? std::string{} : event_data,
                                 destinations);
}

}  // namespace content
