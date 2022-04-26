// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <cstring>
#include <map>
#include <string>

#include "base/check_op.h"
#include "base/guid.h"
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

}  // namespace

FencedFrameURLMapping::PendingAdComponentsMap::PendingAdComponentsMap(
    PendingAdComponentsMap&&) = default;

FencedFrameURLMapping::PendingAdComponentsMap::~PendingAdComponentsMap() =
    default;

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
            .emplace(component_ad.urn, MapInfo(component_ad.url))
            .first;
    it->second.ad_component_urls.emplace();
  }
}

FencedFrameURLMapping::PendingAdComponentsMap::PendingAdComponentsMap(
    const std::vector<GURL>& ad_component_urls) {
  DCHECK_LE(ad_component_urls.size(), blink::kMaxAdAuctionAdComponents);
  for (const GURL& url : ad_component_urls) {
    component_ads_.emplace_back(
        AdComponent{/*urn=*/GenerateURN(), /*url=*/url});
  }

  // Pad `component_ads_` to contain exactly kMaxAdAuctionAdComponents ads, to
  // avoid leaking any data to the fenced frame the component ads array is
  // exposed to.
  while (component_ads_.size() < blink::kMaxAdAuctionAdComponents) {
    component_ads_.emplace_back(
        AdComponent{/*urn=*/GenerateURN(), /*url=*/GURL(url::kAboutBlankURL)});
  }
}

FencedFrameURLMapping::MapInfo::MapInfo() = default;

FencedFrameURLMapping::MapInfo::MapInfo(const GURL& mapped_url)
    : mapped_url(mapped_url) {}

FencedFrameURLMapping::MapInfo::MapInfo(
    const GURL& mapped_url,
    const SharedStorageBudgetMetadata& shared_storage_budget_metadata)
    : mapped_url(mapped_url),
      shared_storage_budget_metadata(shared_storage_budget_metadata) {}

FencedFrameURLMapping::MapInfo::MapInfo(const MapInfo&) = default;
FencedFrameURLMapping::MapInfo::MapInfo(MapInfo&&) = default;
FencedFrameURLMapping::MapInfo::~MapInfo() = default;

FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    const MapInfo&) = default;
FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    MapInfo&&) = default;

FencedFrameURLMapping::FencedFrameURLMapping() = default;
FencedFrameURLMapping::~FencedFrameURLMapping() = default;

GURL FencedFrameURLMapping::AddFencedFrameURL(
    const GURL& url,
    const ReportingMetadata& reporting_metadata) {
  DCHECK(url.is_valid());
  CHECK(blink::IsValidFencedFrameURL(url));

  UrnUuidToUrlMap::iterator it = AddMappingForUrl(url);
  it->second.reporting_metadata = reporting_metadata;
  return it->first;
}

GURL FencedFrameURLMapping::AddFencedFrameURLWithInterestGroupInfo(
    const GURL& url,
    AdAuctionData ad_auction_data,
    std::vector<GURL> ad_component_urls,
    const ReportingMetadata& reporting_metadata) {
  UrnUuidToUrlMap::iterator it = AddMappingForUrl(url);
  it->second.ad_auction_data = std::move(ad_auction_data);
  it->second.ad_component_urls = std::move(ad_component_urls);
  it->second.reporting_metadata = reporting_metadata;
  return it->first;
}

FencedFrameURLMapping::UrnUuidToUrlMap::iterator
FencedFrameURLMapping::AddMappingForUrl(const GURL& url) {
  // Create a urn::uuid.
  GURL urn_uuid = GenerateURN();
  DCHECK(!IsMapped(urn_uuid));

  return urn_uuid_to_url_map_.emplace(urn_uuid, MapInfo(url)).first;
}

GURL FencedFrameURLMapping::GeneratePendingMappedURN() {
  GURL urn_uuid = GenerateURN();
  DCHECK(!IsMapped(urn_uuid));
  DCHECK(!IsPendingMapped(urn_uuid));
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

  absl::optional<GURL> result_url;
  absl::optional<AdAuctionData> result_ad_auction_data;
  absl::optional<PendingAdComponentsMap> result_ad_components;
  ReportingMetadata reporting_metadata;

  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it != urn_uuid_to_url_map_.end()) {
    if (it->second.ad_component_urls) {
      result_ad_components.emplace(
          PendingAdComponentsMap(*it->second.ad_component_urls));
    }
    result_url = it->second.mapped_url;
    result_ad_auction_data = it->second.ad_auction_data;
    reporting_metadata = it->second.reporting_metadata;
  }

  observer->OnFencedFrameURLMappingComplete(
      std::move(result_url), std::move(result_ad_auction_data),
      std::move(result_ad_components), reporting_metadata);
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
  auto it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(it != pending_urn_uuid_to_url_map_.end());

  DCHECK(!IsMapped(urn_uuid));

  absl::optional<GURL> mapped_url = absl::nullopt;

  // Only if the resolved URL is fenced-frame-compatible do we:
  //   1.) Add it to `urn_uuid_to_url_map_`
  //   2.) Report it back to any already-queued observers
  // TODO(crbug.com/1318970): Simplify this by making Shared Storage only
  // capable of producing URLs that fenced frames can navigate to.
  if (blink::IsValidFencedFrameURL(mapping_result.mapped_url)) {
    urn_uuid_to_url_map_.emplace(
        urn_uuid, MapInfo(mapping_result.mapped_url, mapping_result.metadata));
    mapped_url = mapping_result.mapped_url;
  }

  std::set<raw_ptr<MappingResultObserver>>& observers = it->second;

  ReportingMetadata metadata;
  for (raw_ptr<MappingResultObserver> observer : observers) {
    observer->OnFencedFrameURLMappingComplete(
        mapped_url, /*ad_auction_data=*/absl::nullopt,
        /*pending_ad_components_map=*/absl::nullopt,
        /*reporting_metadata=*/metadata);
  }

  pending_urn_uuid_to_url_map_.erase(it);
}

absl::optional<FencedFrameURLMapping::SharedStorageBudgetMetadata>
FencedFrameURLMapping::ReleaseSharedStorageBudgetMetadata(
    const GURL& urn_uuid) {
  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(it != urn_uuid_to_url_map_.end());

  absl::optional<SharedStorageBudgetMetadata> metadata =
      it->second.shared_storage_budget_metadata;

  it->second.shared_storage_budget_metadata.reset();

  return metadata;
}

bool FencedFrameURLMapping::HasObserverForTesting(
    const GURL& urn_uuid,
    MappingResultObserver* observer) {
  return IsPendingMapped(urn_uuid) &&
         pending_urn_uuid_to_url_map_.at(urn_uuid).count(observer);
}

bool FencedFrameURLMapping::IsMapped(const GURL& urn_uuid) const {
  return urn_uuid_to_url_map_.find(urn_uuid) != urn_uuid_to_url_map_.end();
}

bool FencedFrameURLMapping::IsPendingMapped(const GURL& urn_uuid) const {
  return pending_urn_uuid_to_url_map_.find(urn_uuid) !=
         pending_urn_uuid_to_url_map_.end();
}

}  // namespace content
