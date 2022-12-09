// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config_mojom_traits.h"

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame_config.mojom.h"

namespace mojo {

// static
blink::mojom::Opaque
EnumTraits<blink::mojom::Opaque, blink::FencedFrame::Opaque>::ToMojom(
    blink::FencedFrame::Opaque input) {
  switch (input) {
    case blink::FencedFrame::Opaque::kOpaque:
      return blink::mojom::Opaque::kOpaque;
  }
  NOTREACHED();
  return blink::mojom::Opaque::kOpaque;
}

// static
bool EnumTraits<blink::mojom::Opaque, blink::FencedFrame::Opaque>::FromMojom(
    blink::mojom::Opaque input,
    blink::FencedFrame::Opaque* out) {
  switch (input) {
    case blink::mojom::Opaque::kOpaque:
      *out = blink::FencedFrame::Opaque::kOpaque;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
blink::mojom::ReportingDestination
EnumTraits<blink::mojom::ReportingDestination,
           blink::FencedFrame::ReportingDestination>::
    ToMojom(blink::FencedFrame::ReportingDestination input) {
  switch (input) {
    case blink::FencedFrame::ReportingDestination::kBuyer:
      return blink::mojom::ReportingDestination::kBuyer;
    case blink::FencedFrame::ReportingDestination::kSeller:
      return blink::mojom::ReportingDestination::kSeller;
    case blink::FencedFrame::ReportingDestination::kComponentSeller:
      return blink::mojom::ReportingDestination::kComponentSeller;
    case blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl:
      return blink::mojom::ReportingDestination::kSharedStorageSelectUrl;
  }
  NOTREACHED();
  return blink::mojom::ReportingDestination::kBuyer;
}

// static
const base::flat_map<blink::FencedFrame::ReportingDestination,
                     base::flat_map<std::string, GURL>>&
StructTraits<blink::mojom::FencedFrameReportingDataView,
             blink::FencedFrame::FencedFrameReporting>::
    metadata(const blink::FencedFrame::FencedFrameReporting& input) {
  return input.metadata;
}

// static
bool StructTraits<blink::mojom::FencedFrameReportingDataView,
                  blink::FencedFrame::FencedFrameReporting>::
    Read(blink::mojom::FencedFrameReportingDataView data,
         blink::FencedFrame::FencedFrameReporting* out) {
  if (!data.ReadMetadata(&out->metadata)) {
    return false;
  }
  return true;
}

// static
bool EnumTraits<blink::mojom::ReportingDestination,
                blink::FencedFrame::ReportingDestination>::
    FromMojom(blink::mojom::ReportingDestination input,
              blink::FencedFrame::ReportingDestination* out) {
  switch (input) {
    case blink::mojom::ReportingDestination::kBuyer:
      *out = blink::FencedFrame::ReportingDestination::kBuyer;
      return true;
    case blink::mojom::ReportingDestination::kSeller:
      *out = blink::FencedFrame::ReportingDestination::kSeller;
      return true;
    case blink::mojom::ReportingDestination::kComponentSeller:
      *out = blink::FencedFrame::ReportingDestination::kComponentSeller;
      return true;
    case blink::mojom::ReportingDestination::kSharedStorageSelectUrl:
      *out = blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
const url::Origin& StructTraits<blink::mojom::AdAuctionDataDataView,
                                blink::FencedFrame::AdAuctionData>::
    interest_group_owner(const blink::FencedFrame::AdAuctionData& input) {
  return input.interest_group_owner;
}
// static
const std::string& StructTraits<blink::mojom::AdAuctionDataDataView,
                                blink::FencedFrame::AdAuctionData>::
    interest_group_name(const blink::FencedFrame::AdAuctionData& input) {
  return input.interest_group_name;
}

// static
bool StructTraits<blink::mojom::AdAuctionDataDataView,
                  blink::FencedFrame::AdAuctionData>::
    Read(blink::mojom::AdAuctionDataDataView data,
         blink::FencedFrame::AdAuctionData* out_data) {
  if (!data.ReadInterestGroupOwner(&out_data->interest_group_owner) ||
      !data.ReadInterestGroupName(&out_data->interest_group_name)) {
    return false;
  }
  return true;
}

// static
const url::Origin&
StructTraits<blink::mojom::SharedStorageBudgetMetadataDataView,
             blink::FencedFrame::SharedStorageBudgetMetadata>::
    origin(const blink::FencedFrame::SharedStorageBudgetMetadata& input) {
  return input.origin;
}
// static
double StructTraits<blink::mojom::SharedStorageBudgetMetadataDataView,
                    blink::FencedFrame::SharedStorageBudgetMetadata>::
    budget_to_charge(
        const blink::FencedFrame::SharedStorageBudgetMetadata& input) {
  return input.budget_to_charge;
}

// static
bool StructTraits<blink::mojom::SharedStorageBudgetMetadataDataView,
                  blink::FencedFrame::SharedStorageBudgetMetadata>::
    Read(blink::mojom::SharedStorageBudgetMetadataDataView data,
         blink::FencedFrame::SharedStorageBudgetMetadata* out_data) {
  if (!data.ReadOrigin(&out_data->origin)) {
    return false;
  }
  out_data->budget_to_charge = data.budget_to_charge();
  return true;
}

blink::mojom::PotentiallyOpaqueURLPtr
StructTraits<blink::mojom::FencedFrameConfigDataView,
             blink::FencedFrame::RedactedFencedFrameConfig>::
    mapped_url(const blink::FencedFrame::RedactedFencedFrameConfig& config) {
  if (!config.mapped_url_.has_value()) {
    return nullptr;
  }
  if (!config.mapped_url_->potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueURL::NewOpaque(
        blink::FencedFrame::Opaque::kOpaque);
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
        blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueAdAuctionData::NewTransparent(
      *config.ad_auction_data_->potentially_opaque_value);
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
        blink::FencedFrame::Opaque::kOpaque);
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
        NewOpaque(blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
      NewTransparent(
          *config.shared_storage_budget_metadata_->potentially_opaque_value);
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
        blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueReportingMetadata::NewTransparent(
      *config.reporting_metadata_->potentially_opaque_value);
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
          absl::make_optional(ad_auction_data->get_transparent()));
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
      out_config->shared_storage_budget_metadata_.emplace(absl::make_optional(
          shared_storage_budget_metadata->get_transparent()));
    } else {
      out_config->shared_storage_budget_metadata_.emplace(absl::nullopt);
    }
  }
  if (reporting_metadata) {
    if (reporting_metadata->is_transparent()) {
      out_config->reporting_metadata_.emplace(
          absl::make_optional(reporting_metadata->get_transparent()));
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
        blink::FencedFrame::Opaque::kOpaque);
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
        blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueAdAuctionData::NewTransparent(
      *properties.ad_auction_data_->potentially_opaque_value);
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
        blink::FencedFrame::Opaque::kOpaque);
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
        NewOpaque(blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadata::
      NewTransparent(*properties.shared_storage_budget_metadata_
                          ->potentially_opaque_value);
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
        blink::FencedFrame::Opaque::kOpaque);
  }
  return blink::mojom::PotentiallyOpaqueReportingMetadata::NewTransparent(
      *properties.reporting_metadata_->potentially_opaque_value);
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
          absl::make_optional(ad_auction_data->get_transparent()));
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
          absl::make_optional(
              shared_storage_budget_metadata->get_transparent()));
    } else {
      out_properties->shared_storage_budget_metadata_.emplace(absl::nullopt);
    }
  }
  if (reporting_metadata) {
    if (reporting_metadata->is_transparent()) {
      out_properties->reporting_metadata_.emplace(
          absl::make_optional(reporting_metadata->get_transparent()));
    } else {
      out_properties->reporting_metadata_.emplace(absl::nullopt);
    }
  }
  return true;
}

}  // namespace mojo
