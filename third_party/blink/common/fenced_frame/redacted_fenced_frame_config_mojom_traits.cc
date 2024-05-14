// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config_mojom_traits.h"

#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
    case blink::FencedFrame::ReportingDestination::kDirectSeller:
      return blink::mojom::ReportingDestination::kDirectSeller;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::ReportingDestination::kBuyer;
}

// static
blink::mojom::DeprecatedFencedFrameMode
EnumTraits<blink::mojom::DeprecatedFencedFrameMode,
           blink::FencedFrame::DeprecatedFencedFrameMode>::
    ToMojom(blink::FencedFrame::DeprecatedFencedFrameMode input) {
  switch (input) {
    case blink::FencedFrame::DeprecatedFencedFrameMode::kDefault:
      return blink::mojom::DeprecatedFencedFrameMode::kDefault;
    case blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds:
      return blink::mojom::DeprecatedFencedFrameMode::kOpaqueAds;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::DeprecatedFencedFrameMode::kDefault;
}

// static
bool EnumTraits<blink::mojom::DeprecatedFencedFrameMode,
                blink::FencedFrame::DeprecatedFencedFrameMode>::
    FromMojom(blink::mojom::DeprecatedFencedFrameMode input,
              blink::FencedFrame::DeprecatedFencedFrameMode* out) {
  switch (input) {
    case blink::mojom::DeprecatedFencedFrameMode::kDefault:
      *out = blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;
      return true;
    case blink::mojom::DeprecatedFencedFrameMode::kOpaqueAds:
      *out = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
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
    case blink::mojom::ReportingDestination::kDirectSeller:
      *out = blink::FencedFrame::ReportingDestination::kDirectSeller;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
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
const net::SchemefulSite&
StructTraits<blink::mojom::SharedStorageBudgetMetadataDataView,
             blink::FencedFrame::SharedStorageBudgetMetadata>::
    site(const blink::FencedFrame::SharedStorageBudgetMetadata& input) {
  return input.site;
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
    top_navigated(
        const blink::FencedFrame::SharedStorageBudgetMetadata& input) {
  return input.top_navigated;
}

// static
bool StructTraits<blink::mojom::SharedStorageBudgetMetadataDataView,
                  blink::FencedFrame::SharedStorageBudgetMetadata>::
    Read(blink::mojom::SharedStorageBudgetMetadataDataView data,
         blink::FencedFrame::SharedStorageBudgetMetadata* out_data) {
  if (!data.ReadSite(&out_data->site)) {
    return false;
  }
  out_data->budget_to_charge = data.budget_to_charge();
  out_data->top_navigated = data.top_navigated();
  return true;
}

// static
const std::vector<blink::ParsedPermissionsPolicyDeclaration>&
StructTraits<blink::mojom::ParentPermissionsInfoDataView,
             blink::FencedFrame::ParentPermissionsInfo>::
    parsed_permissions_policy(
        const blink::FencedFrame::ParentPermissionsInfo& input) {
  return input.parsed_permissions_policy;
}
// static
const url::Origin& StructTraits<blink::mojom::ParentPermissionsInfoDataView,
                                blink::FencedFrame::ParentPermissionsInfo>::
    origin(const blink::FencedFrame::ParentPermissionsInfo& input) {
  return input.origin;
}

// static
bool StructTraits<blink::mojom::ParentPermissionsInfoDataView,
                  blink::FencedFrame::ParentPermissionsInfo>::
    Read(blink::mojom::ParentPermissionsInfoDataView data,
         blink::FencedFrame::ParentPermissionsInfo* out_data) {
  if (!data.ReadOrigin(&out_data->origin) ||
      !data.ReadParsedPermissionsPolicy(&out_data->parsed_permissions_policy)) {
    return false;
  }
  return true;
}

// static
bool UnionTraits<blink::mojom::PotentiallyOpaqueURLDataView, Prop<GURL>>::Read(
    blink::mojom::PotentiallyOpaqueURLDataView data,
    Prop<GURL>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueURLDataView::Tag::kTransparent: {
      GURL url;
      if (!data.ReadTransparent(&url))
        return false;
      out->potentially_opaque_value.emplace(std::move(url));
      return true;
    }
    case blink::mojom::PotentiallyOpaqueURLDataView::Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueURLDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueURLDataView, Prop<GURL>>::GetTag(
    const Prop<GURL>& property) {
  if (property.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueURLDataView::Tag::kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueURLDataView::Tag::kOpaque;
}

// static
bool UnionTraits<blink::mojom::PotentiallyOpaqueSizeDataView, Prop<gfx::Size>>::
    Read(blink::mojom::PotentiallyOpaqueSizeDataView data,
         Prop<gfx::Size>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueSizeDataView::Tag::kTransparent: {
      gfx::Size size;
      if (!data.ReadTransparent(&size))
        return false;
      out->potentially_opaque_value.emplace(std::move(size));
      return true;
    }
    case blink::mojom::PotentiallyOpaqueSizeDataView::Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueSizeDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueSizeDataView,
            Prop<gfx::Size>>::GetTag(const Prop<gfx::Size>& property) {
  if (property.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueSizeDataView::Tag::kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueSizeDataView::Tag::kOpaque;
}

// static
bool UnionTraits<blink::mojom::PotentiallyOpaqueBoolDataView, Prop<bool>>::Read(
    blink::mojom::PotentiallyOpaqueBoolDataView data,
    Prop<bool>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueBoolDataView::Tag::kTransparent: {
      out->potentially_opaque_value.emplace(data.transparent());
      return true;
    }
    case blink::mojom::PotentiallyOpaqueBoolDataView::Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueBoolDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueBoolDataView, Prop<bool>>::GetTag(
    const Prop<bool>& property) {
  if (property.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueBoolDataView::Tag::kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueBoolDataView::Tag::kOpaque;
}

// static
bool UnionTraits<blink::mojom::PotentiallyOpaqueAdAuctionDataDataView,
                 Prop<blink::FencedFrame::AdAuctionData>>::
    Read(blink::mojom::PotentiallyOpaqueAdAuctionDataDataView data,
         Prop<blink::FencedFrame::AdAuctionData>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueAdAuctionDataDataView::Tag::
        kTransparent: {
      blink::FencedFrame::AdAuctionData ad_auction_data;
      if (!data.ReadTransparent(&ad_auction_data))
        return false;
      out->potentially_opaque_value.emplace(std::move(ad_auction_data));
      return true;
    }
    case blink::mojom::PotentiallyOpaqueAdAuctionDataDataView::Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueAdAuctionDataDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueAdAuctionDataDataView,
            Prop<blink::FencedFrame::AdAuctionData>>::
    GetTag(const Prop<blink::FencedFrame::AdAuctionData>& ad_auction_data) {
  if (ad_auction_data.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueAdAuctionDataDataView::Tag::
        kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueAdAuctionDataDataView::Tag::kOpaque;
}

// static
bool UnionTraits<
    blink::mojom::PotentiallyOpaqueConfigVectorDataView,
    Prop<std::vector<blink::FencedFrame::RedactedFencedFrameConfig>>>::
    Read(
        blink::mojom::PotentiallyOpaqueConfigVectorDataView data,
        Prop<std::vector<blink::FencedFrame::RedactedFencedFrameConfig>>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueConfigVectorDataView::Tag::
        kTransparent: {
      std::vector<blink::FencedFrame::RedactedFencedFrameConfig> config_vector;
      if (!data.ReadTransparent(&config_vector))
        return false;
      out->potentially_opaque_value.emplace(std::move(config_vector));
      return true;
    }
    case blink::mojom::PotentiallyOpaqueConfigVectorDataView::Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueConfigVectorDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueConfigVectorDataView,
            Prop<std::vector<blink::FencedFrame::RedactedFencedFrameConfig>>>::
    GetTag(
        const Prop<std::vector<blink::FencedFrame::RedactedFencedFrameConfig>>&
            config_vector) {
  if (config_vector.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueConfigVectorDataView::Tag::
        kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueConfigVectorDataView::Tag::kOpaque;
}

// static
bool UnionTraits<
    blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView,
    Prop<blink::FencedFrame::SharedStorageBudgetMetadata>>::
    Read(
        blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView data,
        Prop<blink::FencedFrame::SharedStorageBudgetMetadata>* out) {
  switch (data.tag()) {
    case blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView::
        Tag::kTransparent: {
      blink::FencedFrame::SharedStorageBudgetMetadata
          shared_storage_budget_metadata;
      if (!data.ReadTransparent(&shared_storage_budget_metadata))
        return false;
      out->potentially_opaque_value.emplace(
          std::move(shared_storage_budget_metadata));
      return true;
    }
    case blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView::
        Tag::kOpaque: {
      blink::FencedFrame::Opaque opaque;
      if (!data.ReadOpaque(&opaque))
        return false;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView::Tag
UnionTraits<blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView,
            Prop<blink::FencedFrame::SharedStorageBudgetMetadata>>::
    GetTag(const Prop<blink::FencedFrame::SharedStorageBudgetMetadata>&
               shared_storage_budget_metadata) {
  if (shared_storage_budget_metadata.potentially_opaque_value.has_value()) {
    return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView::
        Tag::kTransparent;
  }

  return blink::mojom::PotentiallyOpaqueSharedStorageBudgetMetadataDataView::
      Tag::kOpaque;
}

bool StructTraits<blink::mojom::FencedFrameConfigDataView,
                  blink::FencedFrame::RedactedFencedFrameConfig>::
    Read(blink::mojom::FencedFrameConfigDataView data,
         blink::FencedFrame::RedactedFencedFrameConfig* out_config) {
  GURL urn_uuid;
  if (!data.ReadUrnUuid(&urn_uuid) || !data.ReadMode(&out_config->mode_) ||
      !data.ReadMappedUrl(&out_config->mapped_url_) ||
      !data.ReadContentSize(&out_config->content_size_) ||
      !data.ReadContainerSize(&out_config->container_size_) ||
      !data.ReadDeprecatedShouldFreezeInitialSize(
          &out_config->deprecated_should_freeze_initial_size_) ||
      !data.ReadAdAuctionData(&out_config->ad_auction_data_) ||
      !data.ReadNestedConfigs(&out_config->nested_configs_) ||
      !data.ReadSharedStorageBudgetMetadata(
          &out_config->shared_storage_budget_metadata_) ||
      !data.ReadEffectiveEnabledPermissions(
          &out_config->effective_enabled_permissions_) ||
      !data.ReadParentPermissionsInfo(&out_config->parent_permissions_info_)) {
    return false;
  }

  if (!blink::IsValidUrnUuidURL(urn_uuid)) {
    return false;
  }

  out_config->urn_uuid_ = std::move(urn_uuid);
  return true;
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

bool StructTraits<blink::mojom::FencedFramePropertiesDataView,
                  blink::FencedFrame::RedactedFencedFrameProperties>::
    Read(blink::mojom::FencedFramePropertiesDataView data,
         blink::FencedFrame::RedactedFencedFrameProperties* out_properties) {
  blink::mojom::PotentiallyOpaqueURNConfigVectorPtr nested_urn_config_pairs;
  if (!data.ReadMappedUrl(&out_properties->mapped_url_) ||
      !data.ReadMode(&out_properties->mode_) ||
      !data.ReadContentSize(&out_properties->content_size_) ||
      !data.ReadContainerSize(&out_properties->container_size_) ||
      !data.ReadDeprecatedShouldFreezeInitialSize(
          &out_properties->deprecated_should_freeze_initial_size_) ||
      !data.ReadAdAuctionData(&out_properties->ad_auction_data_) ||
      !data.ReadNestedUrnConfigPairs(&nested_urn_config_pairs) ||
      !data.ReadSharedStorageBudgetMetadata(
          &out_properties->shared_storage_budget_metadata_) ||
      !data.ReadEffectiveEnabledPermissions(
          &out_properties->effective_enabled_permissions_) ||
      !data.ReadParentPermissionsInfo(
          &out_properties->parent_permissions_info_)) {
    return false;
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
      out_properties->nested_urn_config_pairs_.emplace(std::nullopt);
    }
  }

  out_properties->has_fenced_frame_reporting_ =
      data.has_fenced_frame_reporting();

  out_properties->can_disable_untrusted_network_ =
      data.can_disable_untrusted_network();

  out_properties->is_cross_origin_content_ = data.is_cross_origin_content();

  out_properties->allow_cross_origin_event_reporting_ =
      data.allow_cross_origin_event_reporting();
  return true;
}

}  // namespace mojo
