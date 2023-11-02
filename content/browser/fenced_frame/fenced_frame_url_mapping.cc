// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <cstring>
#include <map>
#include <string>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

const char kURNUUIDprefix[] = "urn:uuid:";

GURL GenerateURN() {
  return GURL(kURNUUIDprefix +
              base::GUID::GenerateRandomV4().AsLowercaseString());
}

// Returns a new string based on input where the matching substrings have been
// replaced with the corresponding substitutions. This function avoids repeated
// string operations by building the output based on all substitutions, one
// substitution at a time. This effectively performs all substitutions
// simultaneously, with the earliest match in the input taking precedence.
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

}  // namespace

FencedFrameURLMapping::SharedStorageURNMappingResult::
    SharedStorageURNMappingResult() = default;

FencedFrameURLMapping::SharedStorageURNMappingResult::
    SharedStorageURNMappingResult(GURL mapped_url,
                                  SharedStorageBudgetMetadata budget_metadata,
                                  SharedStorageReportingMap reporting_map)
    : mapped_url(std::move(mapped_url)),
      budget_metadata(std::move(budget_metadata)),
      reporting_map(std::move(reporting_map)) {}

FencedFrameURLMapping::SharedStorageURNMappingResult::
    ~SharedStorageURNMappingResult() = default;

FencedFrameURLMapping::PendingAdComponentsMap::PendingAdComponentsMap(
    const PendingAdComponentsMap&) = default;

FencedFrameURLMapping::PendingAdComponentsMap::PendingAdComponentsMap(
    PendingAdComponentsMap&&) = default;

FencedFrameURLMapping::PendingAdComponentsMap::~PendingAdComponentsMap() =
    default;

FencedFrameURLMapping::PendingAdComponentsMap&
FencedFrameURLMapping::PendingAdComponentsMap::operator=(
    const PendingAdComponentsMap&) = default;

FencedFrameURLMapping::PendingAdComponentsMap&
FencedFrameURLMapping::PendingAdComponentsMap::operator=(
    PendingAdComponentsMap&&) = default;

std::vector<GURL> FencedFrameURLMapping::PendingAdComponentsMap::GetURNs()
    const {
  std::vector<GURL> urns;
  for (const auto& component_ad : component_ads_) {
    urns.push_back(component_ad.urn);
  }
  return urns;
}

void FencedFrameURLMapping::PendingAdComponentsMap::ExportToMapping(
    FencedFrameURLMapping& mapping) const {
  for (const auto& component_ad : component_ads_) {
    DCHECK(!mapping.IsMapped(component_ad.urn));

    UrnUuidToUrlMap::iterator it =
        mapping.urn_uuid_to_url_map_
            .emplace(component_ad.urn, component_ad.config)
            .first;
    it->second.ad_component_configs.emplace();
  }
}

FencedFrameURLMapping::PendingAdComponentsMap::PendingAdComponentsMap(
    const std::vector<MapInfo>& ad_component_configs) {
  DCHECK_LE(ad_component_configs.size(), blink::kMaxAdAuctionAdComponents);
  for (const MapInfo& config : ad_component_configs) {
    component_ads_.emplace_back(
        AdComponent{/*urn=*/GenerateURN(), /*config=*/config});
  }

  // Pad `component_ads_` to contain exactly kMaxAdAuctionAdComponents ads, to
  // avoid leaking any data to the fenced frame the component ads array is
  // exposed to.
  while (component_ads_.size() < blink::kMaxAdAuctionAdComponents) {
    component_ads_.emplace_back(AdComponent{
        /*urn=*/GenerateURN(), /*config=*/MapInfo(GURL(url::kAboutBlankURL))});
  }
}

FencedFrameURLMapping::MapInfo::MapInfo() = default;

FencedFrameURLMapping::MapInfo::MapInfo(const GURL& mapped_url)
    : mapped_url(mapped_url) {}

FencedFrameURLMapping::MapInfo::MapInfo(
    const GURL& mapped_url,
    const SharedStorageBudgetMetadata& shared_storage_budget_metadata,
    const ReportingMetadata& reporting_metadata)
    : mapped_url(mapped_url),
      shared_storage_budget_metadata(shared_storage_budget_metadata),
      reporting_metadata(reporting_metadata) {}

FencedFrameURLMapping::MapInfo::MapInfo(const MapInfo&) = default;
FencedFrameURLMapping::MapInfo::MapInfo(MapInfo&&) = default;
FencedFrameURLMapping::MapInfo::~MapInfo() = default;

FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    const MapInfo&) = default;
FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    MapInfo&&) = default;

FencedFrameURLMapping::FencedFrameProperties::FencedFrameProperties()
    : ad_auction_data(absl::nullopt),
      pending_ad_components_map(absl::nullopt),
      shared_storage_budget_metadata(absl::nullopt),
      partition_nonce(base::UnguessableToken::Create()) {}

FencedFrameURLMapping::FencedFrameProperties::FencedFrameProperties(
    const MapInfo& map_info)
    : mapped_url(map_info.mapped_url),
      ad_auction_data(map_info.ad_auction_data),
      pending_ad_components_map(absl::nullopt),
      shared_storage_budget_metadata(absl::nullopt),
      reporting_metadata(map_info.reporting_metadata),
      partition_nonce(base::UnguessableToken::Create()) {
  if (map_info.shared_storage_budget_metadata) {
    shared_storage_budget_metadata =
        &map_info.shared_storage_budget_metadata.value();
  }
  if (map_info.ad_component_configs) {
    pending_ad_components_map =
        PendingAdComponentsMap(*map_info.ad_component_configs);
  }
}

FencedFrameURLMapping::FencedFrameProperties::FencedFrameProperties(
    const FencedFrameProperties&) = default;
FencedFrameURLMapping::FencedFrameProperties::FencedFrameProperties(
    FencedFrameProperties&&) = default;
FencedFrameURLMapping::FencedFrameProperties::~FencedFrameProperties() =
    default;

FencedFrameURLMapping::FencedFrameProperties&
FencedFrameURLMapping::FencedFrameProperties::operator=(
    const FencedFrameProperties&) = default;
FencedFrameURLMapping::FencedFrameProperties&
FencedFrameURLMapping::FencedFrameProperties::operator=(
    FencedFrameProperties&&) = default;

FencedFrameURLMapping::FencedFrameURLMapping() = default;
FencedFrameURLMapping::~FencedFrameURLMapping() = default;

absl::optional<GURL> FencedFrameURLMapping::AddFencedFrameURL(
    const GURL& url,
    const ReportingMetadata& reporting_metadata) {
  DCHECK(url.is_valid());
  CHECK(blink::IsValidFencedFrameURL(url));

  auto it = AddMappingForUrl(url);

  if (!it.has_value()) {
    // Insertion fails, the number of urn mappings has reached limit.
    return absl::nullopt;
  }

  auto& [urn, map_info] = *it.value();

  map_info.reporting_metadata = reporting_metadata;
  return urn;
}

absl::optional<FencedFrameURLMapping::UrnUuidToUrlMap::iterator>
FencedFrameURLMapping::AddMappingForUrl(const GURL& url) {
  if (IsFull()) {
    // Number of urn mappings has reached limit, url will not be inserted.
    return absl::nullopt;
  }

  // Create a urn::uuid.
  GURL urn_uuid = GenerateURN();
  DCHECK(!IsMapped(urn_uuid));

  return urn_uuid_to_url_map_.emplace(urn_uuid, MapInfo(url)).first;
}

void FencedFrameURLMapping::AssignFencedFrameURLAndInterestGroupInfo(
    const GURL& urn_uuid,
    const GURL& url,
    AdAuctionData ad_auction_data,
    std::vector<GURL> ad_component_urls,
    const ReportingMetadata& reporting_metadata) {
  // The placeholder urn::uuid should have been mapped already.
  DCHECK(IsMapped(urn_uuid));
  auto& map_info = urn_uuid_to_url_map_[urn_uuid];

  // The placeholder urn::uuid should be mapped to an empty URL.
  DCHECK(map_info.mapped_url.is_empty());

  // Assign mapped URL and interest group info.
  map_info.mapped_url = url;
  map_info.ad_auction_data = std::move(ad_auction_data);
  std::vector<MapInfo> ad_component_configs;
  ad_component_configs.reserve(ad_component_urls.size());
  for (auto& ad_component_url : ad_component_urls) {
    ad_component_configs.emplace_back(ad_component_url);
  }
  map_info.ad_component_configs = std::move(ad_component_configs);
  map_info.reporting_metadata = reporting_metadata;
}

absl::optional<GURL> FencedFrameURLMapping::GeneratePlaceholderURN() {
  if (IsFull()) {
    return absl::nullopt;
  }

  GURL urn_uuid = GenerateURN();
  DCHECK(!IsMapped(urn_uuid));
  DCHECK(!IsPendingMapped(urn_uuid));

  urn_uuid_to_url_map_.emplace(urn_uuid, MapInfo());
  return urn_uuid;
}

absl::optional<GURL> FencedFrameURLMapping::GeneratePendingMappedURN() {
  GURL urn_uuid = GenerateURN();
  DCHECK(!IsMapped(urn_uuid));
  DCHECK(!IsPendingMapped(urn_uuid));

  if (IsFull()) {
    return absl::nullopt;
  }

  pending_urn_uuid_to_url_map_.emplace(
      urn_uuid, std::set<raw_ptr<MappingResultObserver>>());
  return urn_uuid;
}

void FencedFrameURLMapping::ConvertFencedFrameURNToURL(
    const GURL& urn_uuid,
    MappingResultObserver* observer) {
  DCHECK(blink::IsValidUrnUuidURL(urn_uuid));

  if (IsPendingMapped(urn_uuid)) {
    DCHECK(!pending_urn_uuid_to_url_map_.at(urn_uuid).count(observer));
    pending_urn_uuid_to_url_map_.at(urn_uuid).emplace(observer);
    return;
  }

  absl::optional<FencedFrameProperties> properties;

  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it != urn_uuid_to_url_map_.end()) {
    properties = FencedFrameProperties(it->second);
  }

  observer->OnFencedFrameURLMappingComplete(properties);
}

void FencedFrameURLMapping::RemoveObserverForURN(
    const GURL& urn_uuid,
    MappingResultObserver* observer) {
  auto it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(it != pending_urn_uuid_to_url_map_.end());

  auto observer_it = it->second.find(observer);
  DCHECK(observer_it != it->second.end());

  it->second.erase(observer_it);
}

void FencedFrameURLMapping::OnSharedStorageURNMappingResultDetermined(
    const GURL& urn_uuid,
    const SharedStorageURNMappingResult& mapping_result) {
  auto pending_it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(pending_it != pending_urn_uuid_to_url_map_.end());

  DCHECK(!IsMapped(urn_uuid));

  absl::optional<MapInfo> config = absl::nullopt;

  // Only if the resolved URL is fenced-frame-compatible do we:
  //   1.) Add it to `urn_uuid_to_url_map_`
  //   2.) Report it back to any already-queued observers
  // TODO(crbug.com/1318970): Simplify this by making Shared Storage only
  // capable of producing URLs that fenced frames can navigate to.
  if (blink::IsValidFencedFrameURL(mapping_result.mapped_url)) {
    ReportingMetadata reporting_metadata;
    base::flat_map<blink::mojom::ReportingDestination,
                   SharedStorageReportingMap>
        reporting_metadata_map;
    reporting_metadata_map
        [blink::mojom::ReportingDestination::kSharedStorageSelectUrl] =
            std::move(mapping_result.reporting_map);
    reporting_metadata = ReportingMetadata(std::move(reporting_metadata_map));

    config = MapInfo(mapping_result.mapped_url, mapping_result.budget_metadata,
                     reporting_metadata);
    urn_uuid_to_url_map_.emplace(urn_uuid, *config);
  }

  std::set<raw_ptr<MappingResultObserver>>& observers = pending_it->second;

  absl::optional<FencedFrameProperties> properties = absl::nullopt;
  auto final_it = urn_uuid_to_url_map_.find(urn_uuid);
  if (final_it != urn_uuid_to_url_map_.end()) {
    properties = FencedFrameProperties(final_it->second);
  }

  for (raw_ptr<MappingResultObserver> observer : observers) {
    observer->OnFencedFrameURLMappingComplete(properties);
  }

  pending_urn_uuid_to_url_map_.erase(pending_it);
}

FencedFrameURLMapping::SharedStorageBudgetMetadata*
FencedFrameURLMapping::GetSharedStorageBudgetMetadata(const GURL& urn_uuid) {
  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(it != urn_uuid_to_url_map_.end());

  if (!it->second.shared_storage_budget_metadata)
    return nullptr;

  return &it->second.shared_storage_budget_metadata.value();
}

void FencedFrameURLMapping::SubstituteMappedURL(
    const GURL& urn_uuid,
    const std::vector<std::pair<std::string, std::string>>& substitutions) {
  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it == urn_uuid_to_url_map_.end()) {
    return;
  }
  MapInfo info = it->second;
  info.mapped_url = GURL(
      SubstituteMappedStrings(it->second.mapped_url.spec(), substitutions));
  if (!info.mapped_url.is_valid()) {
    return;
  }
  if (info.ad_component_configs) {
    for (auto& ad_component_config : info.ad_component_configs.value()) {
      GURL substituted_url = GURL(SubstituteMappedStrings(
          ad_component_config.mapped_url.spec(), substitutions));
      if (!substituted_url.is_valid()) {
        return;
      }
      ad_component_config = MapInfo(substituted_url);
    }
  }
  it->second = std::move(info);
}

bool FencedFrameURLMapping::IsMapped(const GURL& urn_uuid) const {
  return urn_uuid_to_url_map_.find(urn_uuid) != urn_uuid_to_url_map_.end();
}

bool FencedFrameURLMapping::IsPendingMapped(const GURL& urn_uuid) const {
  return pending_urn_uuid_to_url_map_.find(urn_uuid) !=
         pending_urn_uuid_to_url_map_.end();
}

bool FencedFrameURLMapping::IsFull() const {
  return urn_uuid_to_url_map_.size() + pending_urn_uuid_to_url_map_.size() >=
         kMaxUrnMappingSize;
}

}  // namespace content
