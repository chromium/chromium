// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config_mojom_traits.h"

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom.h"

namespace mojo {

blink::mojom::PotentiallyOpaqueURLPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    mapped_url(const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.mapped_url_.has_value()) {
    return nullptr;
  }
  if (!config.mapped_url_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueURL::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueURL::NewTransparent(
      *config.mapped_url_->potentially_opaque_value);
}

blink::mojom::PotentiallyOpaqueAdAuctionDataPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    ad_auction_data(
        const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.ad_auction_data_.has_value()) {
    return nullptr;
  }
  if (!config.ad_auction_data_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueAdAuctionData::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueAdAuctionData::NewTransparent(
      blink::mojom::AdAuctionData::New(
          config.ad_auction_data_->potentially_opaque_value
              ->interest_group_owner,
          config.ad_auction_data_->potentially_opaque_value
              ->interest_group_name));
}

blink::mojom::PotentiallyOpaqueConfigVectorPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    nested_configs(
        const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.nested_configs_.has_value()) {
    return nullptr;
  }
  if (!config.nested_configs_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueConfigVector::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  auto nested_config_vector =
      blink::mojom::PotentiallyOpaqueConfigVector::NewTransparent({});
  for (auto& nested_config :
       config.nested_configs_->potentially_opaque_value.value()) {
    nested_config_vector->get_transparent().push_back(nested_config);
  }
  return nested_config_vector;
}

blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    shared_storage_budget_metadata(
        const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.shared_storage_budget_metadata_.has_value()) {
    return nullptr;
  }
  if (!config.shared_storage_budget_metadata_->potentially_opaque_value
           .has_value()) {
    return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
        NewOpaque(blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
      NewTransparent(blink::mojom::SharedStorageBudgetMetadata::New(
          config.shared_storage_budget_metadata_->potentially_opaque_value
              ->origin,
          config.shared_storage_budget_metadata_->potentially_opaque_value
              ->budget_to_charge));
}

blink::mojom::PotentiallyOpaqueReportingMetadataPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    reporting_metadata(
        const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.reporting_metadata_.has_value()) {
    return nullptr;
  }
  if (!config.reporting_metadata_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueReportingMetadata::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueReportingMetadata::NewTransparent(
      config.reporting_metadata_->potentially_opaque_value->Clone());
}

bool StructTraits<blink::mojom::FencedFrameConfigDataView,
                  blink::FencedFrame::RedactedFencedFrameConfig>::
    Read(blink::mojom::FencedFrameConfigDataView data,
         blink::FencedFrame::RedactedFencedFrameConfig* out_config) {
  blink::mojom::PotentiallyOpaqueURLPtr mapped_url;
  blink::mojom::PotentiallyOpaqueAdAuctionDataPtr ad_auction_data;
  blink::mojom::PotentiallyOpaqueConfigVectorPtr nested_configs;
  blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
      shared_storage_budget_metadata;
  blink::mojom::PotentiallyOpaqueReportingMetadataPtr reporting_metadata;
  if (!data.ReadMappedUrl(&mapped_url) ||
      !data.ReadAdAuctionData(&ad_auction_data) ||
      !data.ReadNestedConfigs(&nested_configs) ||
      !data.ReadSharedStorageBudgetMetadata(&shared_storage_budget_metadata) ||
      !data.ReadReportingMetadata(&reporting_metadata)) {
    return false;
  }

  if (mapped_url) {
    if (mapped_url->is_transparent()) {
      out_config->mapped_url_.emplace(
          absl::make_optional(mapped_url->get_transparent()));
    } else {
      out_config->mapped_url_.emplace(absl::nullopt);
    }
  }
  if (ad_auction_data) {
    if (ad_auction_data->is_transparent()) {
      out_config->ad_auction_data_.emplace(
          absl::make_optional(blink::FencedFrame::AdAuctionData{
              ad_auction_data->get_transparent()->interest_group_owner,
              ad_auction_data->get_transparent()->interest_group_name}));
    } else {
      out_config->ad_auction_data_.emplace(absl::nullopt);
    }
  }
  if (nested_configs) {
    if (nested_configs->is_transparent()) {
      out_config->nested_configs_.emplace(
          std::vector<blink::FencedFrame::RedactedFencedFrameConfig>());
      for (auto& nested_config : nested_configs->get_transparent()) {
        out_config->nested_configs_->potentially_opaque_value->push_back(
            nested_config);
      }
    } else {
      out_config->nested_configs_.emplace(absl::nullopt);
    }
  }
  if (shared_storage_budget_metadata) {
    if (shared_storage_budget_metadata->is_transparent()) {
      out_config->shared_storage_budget_metadata_.emplace(
          absl::make_optional(blink::FencedFrame::SharedStorageBudgetMetadata{
              shared_storage_budget_metadata->get_transparent()->origin,
              shared_storage_budget_metadata->get_transparent()
                  ->budget_to_charge}));
    } else {
      out_config->shared_storage_budget_metadata_.emplace(absl::nullopt);
    }
  }
  if (reporting_metadata) {
    if (reporting_metadata->is_transparent()) {
      out_config->reporting_metadata_.emplace(
          absl::make_optional(*reporting_metadata->get_transparent()));
    } else {
      out_config->reporting_metadata_.emplace(absl::nullopt);
    }
  }
  return true;
}

blink::mojom::PotentiallyOpaqueURLPtr
StructTraits<blink::mojom::FencedFramePropertiesDataView,
             blink::FencedFrame::RedactedFencedFrameProperties>::
    mapped_url(
        const blink::FencedFrame::RedactedFencedFrameProperties& properties) {
  if (!properties.mapped_url_.has_value()) {
    return nullptr;
  }
  if (!properties.mapped_url_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueURL::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueURL::NewTransparent(
      *properties.mapped_url_->potentially_opaque_value);
}

blink::mojom::PotentiallyOpaqueAdAuctionDataPtr
StructTraits<blink::mojom::FencedFramePropertiesDataView,
             blink::FencedFrame::RedactedFencedFrameProperties>::
    ad_auction_data(
        const blink::FencedFrame::RedactedFencedFrameProperties& properties) {
  if (!properties.ad_auction_data_.has_value()) {
    return nullptr;
  }
  if (!properties.ad_auction_data_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueAdAuctionData::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueAdAuctionData::NewTransparent(
      blink::mojom::AdAuctionData::New(
          properties.ad_auction_data_->potentially_opaque_value
              ->interest_group_owner,
          properties.ad_auction_data_->potentially_opaque_value
              ->interest_group_name));
}

blink::mojom::PotentiallyOpaqueURNConfigVectorPtr
StructTraits<blink::mojom::FencedFramePropertiesDataView,
             blink::FencedFrame::RedactedFencedFrameProperties>::
    nested_urn_config_pairs(
        const blink::FencedFrame::RedactedFencedFrameProperties& properties) {
  if (!properties.nested_urn_config_pairs_.has_value()) {
    return nullptr;
  }
  if (!properties.nested_urn_config_pairs_->potentially_opaque_value
           .has_value()) {
    return blink::mojom::PotentiallyOpaqueURNConfigVector::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  auto nested_urn_config_vector =
      blink::mojom::PotentiallyOpaqueURNConfigVector::NewTransparent({});
  for (auto& nested_urn_config_pair :
       properties.nested_urn_config_pairs_->potentially_opaque_value.value()) {
    nested_urn_config_vector->get_transparent().push_back(
        blink::mojom::URNConfigPair::New(nested_urn_config_pair.first,
                                         nested_urn_config_pair.second));
  }
  return nested_urn_config_vector;
}

blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
StructTraits<blink::mojom::FencedFramePropertiesDataView,
             blink::FencedFrame::RedactedFencedFrameProperties>::
    shared_storage_budget_metadata(
        const blink::FencedFrame::RedactedFencedFrameProperties& properties) {
  if (!properties.shared_storage_budget_metadata_.has_value()) {
    return nullptr;
  }
  if (!properties.shared_storage_budget_metadata_->potentially_opaque_value
           .has_value()) {
    return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
        NewOpaque(blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
      NewTransparent(blink::mojom::SharedStorageBudgetMetadata::New(
          properties.shared_storage_budget_metadata_->potentially_opaque_value
              ->origin,
          properties.shared_storage_budget_metadata_->potentially_opaque_value
              ->budget_to_charge));
}

blink::mojom::PotentiallyOpaqueReportingMetadataPtr
StructTraits<blink::mojom::FencedFramePropertiesDataView,
             blink::FencedFrame::RedactedFencedFrameProperties>::
    reporting_metadata(
        const blink::FencedFrame::RedactedFencedFrameProperties& properties) {
  if (!properties.reporting_metadata_.has_value()) {
    return nullptr;
  }
  if (!properties.reporting_metadata_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueReportingMetadata::NewOpaque(
        blink::mojom::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueReportingMetadata::NewTransparent(
      properties.reporting_metadata_->potentially_opaque_value->Clone());
}

bool StructTraits<blink::mojom::FencedFramePropertiesDataView,
                  blink::FencedFrame::RedactedFencedFrameProperties>::
    Read(blink::mojom::FencedFramePropertiesDataView data,
         blink::FencedFrame::RedactedFencedFrameProperties* out_properties) {
  blink::mojom::PotentiallyOpaqueURLPtr mapped_url;
  blink::mojom::PotentiallyOpaqueAdAuctionDataPtr ad_auction_data;
  blink::mojom::PotentiallyOpaqueURNConfigVectorPtr nested_urn_config_pairs;
  blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataPtr
      shared_storage_budget_metadata;
  blink::mojom::PotentiallyOpaqueReportingMetadataPtr reporting_metadata;
  if (!data.ReadMappedUrl(&mapped_url) ||
      !data.ReadAdAuctionData(&ad_auction_data) ||
      !data.ReadNestedUrnConfigPairs(&nested_urn_config_pairs) ||
      !data.ReadSharedStorageBudgetMetadata(&shared_storage_budget_metadata) ||
      !data.ReadReportingMetadata(&reporting_metadata)) {
    return false;
  }
  if (mapped_url) {
    if (mapped_url->is_transparent()) {
      out_properties->mapped_url_.emplace(
          absl::make_optional(mapped_url->get_transparent()));
    } else {
      out_properties->mapped_url_.emplace(absl::nullopt);
    }
  }
  if (ad_auction_data) {
    if (ad_auction_data->is_transparent()) {
      out_properties->ad_auction_data_.emplace(
          absl::make_optional(blink::FencedFrame::AdAuctionData{
              ad_auction_data->get_transparent()->interest_group_owner,
              ad_auction_data->get_transparent()->interest_group_name}));
    } else {
      out_properties->ad_auction_data_.emplace(absl::nullopt);
    }
  }
  if (nested_urn_config_pairs) {
    if (nested_urn_config_pairs->is_transparent()) {
      out_properties->nested_urn_config_pairs_.emplace(
          std::vector<std::pair<
              GURL, blink::FencedFrame::RedactedFencedFrameConfig>>());
      for (auto& nested_urn_config_pair :
           nested_urn_config_pairs->get_transparent()) {
        out_properties->nested_urn_config_pairs_->potentially_opaque_value
            ->emplace_back(nested_urn_config_pair->urn,
                           nested_urn_config_pair->config);
      }
    } else {
      out_properties->nested_urn_config_pairs_.emplace(absl::nullopt);
    }
  }
  if (shared_storage_budget_metadata) {
    if (shared_storage_budget_metadata->is_transparent()) {
      out_properties->shared_storage_budget_metadata_.emplace(
          absl::make_optional(blink::FencedFrame::SharedStorageBudgetMetadata{
              shared_storage_budget_metadata->get_transparent()->origin,
              shared_storage_budget_metadata->get_transparent()
                  ->budget_to_charge}));
    } else {
      out_properties->shared_storage_budget_metadata_.emplace(absl::nullopt);
    }
  }
  if (reporting_metadata) {
    if (reporting_metadata->is_transparent()) {
      out_properties->reporting_metadata_.emplace(
          absl::make_optional(*reporting_metadata->get_transparent()));
    } else {
      out_properties->reporting_metadata_.emplace(absl::nullopt);
    }
  }
  return true;
}

}  // namespace mojo
