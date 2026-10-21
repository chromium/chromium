// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "url/gurl.h"

namespace content {

FencedFrameURLMapping::FencedFrameURLMapping() = default;

FencedFrameURLMapping::~FencedFrameURLMapping() = default;

void FencedFrameURLMapping::ImportPendingAdComponents(
    const std::vector<std::pair<GURL, FencedFrameConfig>>& components) {
  for (const auto& component_ad : components) {
    // If this is called redundantly, do nothing.
    // This happens in urn iframes, because the FencedFrameURLMapping is
    // attached to the Page. In fenced frames, the Page is rooted at the fenced
    // frame root, so a new FencedFrameURLMapping is created when the root is
    // navigated. In urn iframes, the Page is rooted at the top-level frame, so
    // the same FencedFrameURLMapping exists after "urn iframe root"
    // navigations.
    // TODO(crbug.com/40256574): Change this to a CHECK when we remove urn
    // iframes.
    if (IsMapped(component_ad.first)) {
      return;
    }

    UrnUuidToUrlMap::iterator it =
        urn_uuid_to_url_map_.emplace(component_ad.first, component_ad.second)
            .first;
    it->second.nested_configs_.emplace(std::vector<FencedFrameConfig>(),
                                       VisibilityToEmbedder::kTransparent,
                                       VisibilityToContent::kTransparent);
  }
}

std::optional<GURL> FencedFrameURLMapping::AddFencedFrameURLForTesting(
    const GURL& url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  CHECK(url.is_valid(), base::NotFatalUntil::M160);
  CHECK(blink::IsValidFencedFrameURL(url));

  auto it = AddMappingForUrl(url);

  if (!it.has_value()) {
    // Insertion fails, the number of urn mappings has reached limit.
    return std::nullopt;
  }

  auto& [urn, config] = *it.value();

  config.fenced_frame_reporter_ = std::move(fenced_frame_reporter);
  config.mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
  // Give this frame the more restrictive option.
  config.allows_information_inflow_ = false;
  config.deprecated_should_freeze_initial_size_.emplace(
      true, VisibilityToEmbedder::kTransparent, VisibilityToContent::kOpaque);
  return urn;
}

void FencedFrameURLMapping::ClearMapForTesting() {
  urn_uuid_to_url_map_.clear();
}

std::optional<FencedFrameURLMapping::UrnUuidToUrlMap::iterator>
FencedFrameURLMapping::AddMappingForUrl(const GURL& url) {
  if (IsFull()) {
    // Number of urn mappings has reached limit, url will not be inserted.
    return std::nullopt;
  }

  // Create a urn::uuid.
  GURL urn_uuid = GenerateUrnUuid();
  CHECK(!IsMapped(urn_uuid), base::NotFatalUntil::M160);

  return urn_uuid_to_url_map_
      .emplace(urn_uuid, FencedFrameConfig(urn_uuid, url))
      .first;
}

void FencedFrameURLMapping::ConvertFencedFrameURNToURL(
    const GURL& urn_uuid,
    MappingResultObserver* observer) {
  CHECK(blink::IsValidUrnUuidURL(urn_uuid), base::NotFatalUntil::M160);

  std::optional<FencedFrameProperties> properties;

  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it != urn_uuid_to_url_map_.end()) {
    properties = FencedFrameProperties(it->second);
  }

  if (properties.has_value() && properties->ad_auction_data().has_value()) {
    base::UmaHistogramBoolean("Ads.InterestGroup.Auction.AdNavigationStarted",
                              true);
  }

  observer->OnFencedFrameURLMappingComplete(properties);
}

bool FencedFrameURLMapping::IsMapped(const GURL& urn_uuid) const {
  return urn_uuid_to_url_map_.contains(urn_uuid);
}

bool FencedFrameURLMapping::IsFull() const {
  return urn_uuid_to_url_map_.size() >= kMaxUrnMappingSize;
}

}  // namespace content
