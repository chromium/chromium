// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <map>
#include <string>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

GURL GenerateURN() {
  return GURL(kURNUUIDprefix + base::UnguessableToken::Create().ToString());
}

}  // namespace

const char kURNUUIDprefix[] = "urn:uuid:";

bool FencedFrameURLMapping::IsValidUrnUuidURL(const GURL& url) {
  if (!url.is_valid())
    return false;
  std::string spec = url.spec();
  return base::StartsWith(spec, kURNUUIDprefix,
                          base::CompareCase::INSENSITIVE_ASCII);
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
    CHECK(!mapping.IsPresent(component_ad.urn));

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

absl::optional<GURL> FencedFrameURLMapping::ConvertFencedFrameURNToURL(
    const GURL& urn_uuid,
    absl::optional<PendingAdComponentsMap>& out_ad_components) const {
  CHECK(IsValidUrnUuidURL(urn_uuid));

  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it == urn_uuid_to_url_map_.end())
    return absl::nullopt;

  if (it->second.ad_component_urls) {
    out_ad_components.emplace(
        PendingAdComponentsMap(*it->second.ad_component_urls));
  }
  return it->second.mapped_url;
}

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
  CHECK(!IsPresent(urn_uuid));

  return urn_uuid_to_url_map_.emplace(urn_uuid, MapInfo(url)).first;
}

bool FencedFrameURLMapping::IsPresent(const GURL& urn_uuid) {
  return urn_uuid_to_url_map_.find(urn_uuid) != urn_uuid_to_url_map_.end();
}

}  // namespace content
