// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <map>
#include <string>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

GURL GenerateURN() {
  return GURL(kURNUUIDprefix +
              base::GUID::GenerateRandomV4().AsLowercaseString());
}

}  // namespace

const char kURNUUIDprefix[] = "urn:uuid:";
const int kURNUUIDDashLocations[4] = {17, 22, 27, 32};

bool FencedFrameURLMapping::IsValidUrnUuidURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  std::string spec = url.spec();
  return base::StartsWith(spec, kURNUUIDprefix,
                          base::CompareCase::INSENSITIVE_ASCII) &&
         spec.at(kURNUUIDDashLocations[0]) == '-' &&
         spec.at(kURNUUIDDashLocations[1]) == '-' &&
         spec.at(kURNUUIDDashLocations[2]) == '-' &&
         spec.at(kURNUUIDDashLocations[3]) == '-';
}

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

FencedFrameURLMapping::MapInfo::MapInfo(const MapInfo&) = default;
FencedFrameURLMapping::MapInfo::MapInfo(MapInfo&&) = default;
FencedFrameURLMapping::MapInfo::~MapInfo() = default;

FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    const MapInfo&) = default;
FencedFrameURLMapping::MapInfo& FencedFrameURLMapping::MapInfo::operator=(
    MapInfo&&) = default;

FencedFrameURLMapping::FencedFrameURLMapping() = default;
FencedFrameURLMapping::~FencedFrameURLMapping() = default;

GURL FencedFrameURLMapping::AddFencedFrameURL(const GURL& url) {
  DCHECK(url.is_valid());
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));

  return AddMappingForUrl(url)->first;
}

GURL FencedFrameURLMapping::AddFencedFrameURLWithInterestGroupAdComponentUrls(
    const GURL& url,
    std::vector<GURL> ad_component_urls) {
  UrnUuidToUrlMap::iterator it = AddMappingForUrl(url);
  it->second.ad_component_urls = std::move(ad_component_urls);
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
  DCHECK(IsValidUrnUuidURL(urn_uuid));

  if (IsPendingMapped(urn_uuid)) {
    DCHECK(!pending_urn_uuid_to_url_map_.at(urn_uuid).count(observer));
    pending_urn_uuid_to_url_map_.at(urn_uuid).emplace(observer);
    return;
  }

  absl::optional<GURL> result_url;
  absl::optional<PendingAdComponentsMap> result_ad_components;

  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it != urn_uuid_to_url_map_.end()) {
    if (it->second.ad_component_urls) {
      result_ad_components.emplace(
          PendingAdComponentsMap(*it->second.ad_component_urls));
    }
    result_url = it->second.mapped_url;
  }

  observer->OnFencedFrameURLMappingComplete(std::move(result_url),
                                            std::move(result_ad_components));
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

void FencedFrameURLMapping::OnURNMappingResultDetermined(
    const GURL& urn_uuid,
    const absl::optional<GURL>& mapped_url) {
  auto it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(it != pending_urn_uuid_to_url_map_.end());

  DCHECK(!IsMapped(urn_uuid));

  if (mapped_url)
    urn_uuid_to_url_map_.emplace(urn_uuid, mapped_url.value());

  std::set<raw_ptr<MappingResultObserver>>& observers = it->second;

  for (raw_ptr<MappingResultObserver> observer : observers) {
    observer->OnFencedFrameURLMappingComplete(
        mapped_url,
        /*pending_ad_components_map=*/absl::nullopt);
  }

  pending_urn_uuid_to_url_map_.erase(it);
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
