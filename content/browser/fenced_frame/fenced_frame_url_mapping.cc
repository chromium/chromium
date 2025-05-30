// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/permissions_policy/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace content {

namespace {

int AdSizeToPixels(double size, blink::AdSize::LengthUnit unit) {
  switch (unit) {
    case blink::AdSize::LengthUnit::kPixels:
      return static_cast<int>(size);
    case blink::AdSize::LengthUnit::kScreenWidth: {
      double screen_width = display::Screen::GetScreen()
                                ->GetPrimaryDisplay()
                                .GetSizeInPixel()
                                .width();
      return static_cast<int>(size / 100.0 * screen_width);
    }
    case blink::AdSize::LengthUnit::kScreenHeight: {
      double screen_height = display::Screen::GetScreen()
                                 ->GetPrimaryDisplay()
                                 .GetSizeInPixel()
                                 .height();
      return static_cast<int>(size / 100.0 * screen_height);
    }
    case blink::AdSize::LengthUnit::kInvalid:
      NOTREACHED();
  }
}

gfx::Size AdSizeToGfxSize(const blink::AdSize& ad_size) {
  int width_in_pixels = AdSizeToPixels(ad_size.width, ad_size.width_units);
  int height_in_pixels = AdSizeToPixels(ad_size.height, ad_size.height_units);

  return gfx::Size(width_in_pixels, height_in_pixels);
}

// TODO(crbug.com/40258855): Once the representation of size in fenced frame
// config is finalized, change the type of substituted width and height to the
// same.
// Substitute the size macros in ad url with the size from the winning bid.
GURL SubstituteSizeIntoURL(const blink::AdDescriptor& ad_descriptor) {
  if (!ad_descriptor.size) {
    return ad_descriptor.url;
  }

  // Convert dimensions to pixels.
  gfx::Size size = AdSizeToGfxSize(ad_descriptor.size.value());

  std::string width = base::NumberToString(size.width());
  std::string height = base::NumberToString(size.height());
  std::vector<std::pair<std::string, std::string>> substitutions;

  // Set up the width and height macros, in two formats.
  substitutions.emplace_back("{%AD_WIDTH%}", width);
  substitutions.emplace_back("{%AD_HEIGHT%}", height);
  substitutions.emplace_back("${AD_WIDTH}", width);
  substitutions.emplace_back("${AD_HEIGHT}", height);

  return GURL(SubstituteMappedStrings(ad_descriptor.url.spec(), substitutions));
}

// For each ad component and its parent, we examine its SchemefulSite, and
// increment a count of how many ads correspond to that site. We then log the
// highest number of ads which are same-site to one another (the largest value
// in the map below) as a UMA metric. The purpose of this metric is to explore
// how often ads are same-site with one another, which may inform future limits
// on same-site fenced frames. We also log a separate UMA metric to count the
// total number of ad components regardless of their site.
class AdComponentMetrics {
 public:
  explicit AdComponentMetrics(int total_component_count)
      : total_component_count_(total_component_count) {}

  void UpdateForSite(net::SchemefulSite component_site) {
    auto iter = components_per_site_counts_.find(component_site);
    if (iter == components_per_site_counts_.end()) {
      components_per_site_counts_.insert({component_site, 1});
    } else {
      iter->second += 1;
    }
    int count_for_current_site =
        iter == components_per_site_counts_.end() ? 1 : iter->second;
    if (count_for_current_site > same_site_component_max_count_) {
      same_site_component_max_count_ = count_for_current_site;
    }
  }

  void LogSameSiteMetric() {
    base::UmaHistogramExactLinear(
        blink::kSameSiteAdComponentsMaxCountForWinningBidHistogram,
        same_site_component_max_count_,
        // We shouldn't allow the histogram bucket count to change over time, so
        // assume the largest allowed number of components per auction.
        blink::kMaxAdAuctionAdComponentsConfigLimit + 1);
  }

  void LogTotalCountMetric() {
    base::UmaHistogramExactLinear(
        blink::kAdComponentsCountForWinningBidHistogram, total_component_count_,
        blink::kMaxAdAuctionAdComponentsConfigLimit + 1);
  }

 private:
  std::map<net::SchemefulSite, int> components_per_site_counts_;
  int same_site_component_max_count_ = 0;
  int total_component_count_ = 0;
};

}  // namespace

FencedFrameURLMapping::FencedFrameURLMapping() = default;

FencedFrameURLMapping::~FencedFrameURLMapping() = default;

FencedFrameURLMapping::SharedStorageURNMappingResult::
    SharedStorageURNMappingResult() = default;

FencedFrameURLMapping::SharedStorageURNMappingResult::
    SharedStorageURNMappingResult(
        GURL mapped_url,
        SharedStorageBudgetMetadata budget_metadata,
        scoped_refptr<FencedFrameReporter> fenced_frame_reporter)
    : mapped_url(std::move(mapped_url)),
      budget_metadata(std::move(budget_metadata)),
      fenced_frame_reporter(std::move(fenced_frame_reporter)) {}

FencedFrameURLMapping::SharedStorageURNMappingResult::
    ~SharedStorageURNMappingResult() = default;

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
  DCHECK(url.is_valid());
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
  // We don't know at this point if the test being run needs the FLEDGE or
  // Shared Storage permissions set. To be safe, we set both here.
  config.effective_enabled_permissions_.insert(
      config.effective_enabled_permissions_.end(),
      std::begin(network::kFencedFrameFledgeDefaultRequiredFeatures),
      std::end(network::kFencedFrameFledgeDefaultRequiredFeatures));
  config.effective_enabled_permissions_.insert(
      config.effective_enabled_permissions_.end(),
      std::begin(network::kFencedFrameSharedStorageDefaultRequiredFeatures),
      std::end(network::kFencedFrameSharedStorageDefaultRequiredFeatures));
  return urn;
}

void FencedFrameURLMapping::ClearMapForTesting() {
  urn_uuid_to_url_map_.clear();
  pending_urn_uuid_to_url_map_.clear();
}

std::optional<FencedFrameURLMapping::UrnUuidToUrlMap::iterator>
FencedFrameURLMapping::AddMappingForUrl(const GURL& url) {
  if (IsFull()) {
    // Number of urn mappings has reached limit, url will not be inserted.
    return std::nullopt;
  }

  // Create a urn::uuid.
  GURL urn_uuid = GenerateUrnUuid();
  DCHECK(!IsMapped(urn_uuid));

  return urn_uuid_to_url_map_
      .emplace(urn_uuid, FencedFrameConfig(urn_uuid, url))
      .first;
}

blink::FencedFrame::RedactedFencedFrameConfig
FencedFrameURLMapping::AssignFencedFrameURLAndInterestGroupInfo(
    const GURL& urn_uuid,
    std::optional<blink::AdSize> container_size,
    const blink::AdDescriptor& ad_descriptor,
    AdAuctionData ad_auction_data,
    base::RepeatingClosure on_navigate_callback,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  // Move pending mapped urn::uuid to `urn_uuid_to_url_map_`.
  // TODO(crbug.com/40896818): Remove the check for whether `urn_uuid` has been
  // mapped already once the crash is resolved.
  CHECK(!IsMapped(urn_uuid));
  auto pending_it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  CHECK(pending_it != pending_urn_uuid_to_url_map_.end());
  pending_urn_uuid_to_url_map_.erase(pending_it);

  bool emplaced = false;
  std::tie(std::ignore, emplaced) =
      urn_uuid_to_url_map_.emplace(urn_uuid, FencedFrameConfig());
  DCHECK(emplaced);
  auto& config = urn_uuid_to_url_map_[urn_uuid];

  // Assign mapped URL and interest group info.
  // TODO(crbug.com/40258855): Once the representation of size in fenced frame
  // config is finalized, pass the ad size from the winning bid to its fenced
  // frame config.
  config.urn_uuid_.emplace(urn_uuid);
  config.mapped_url_.emplace(SubstituteSizeIntoURL(ad_descriptor),
                             VisibilityToEmbedder::kOpaque,
                             VisibilityToContent::kTransparent);
  if (container_size.has_value() &&
      blink::IsValidAdSize(container_size.value())) {
    gfx::Size container_gfx_size = AdSizeToGfxSize(container_size.value());
    config.container_size_.emplace(container_gfx_size,
                                   VisibilityToEmbedder::kTransparent,
                                   VisibilityToContent::kOpaque);
  }
  if (ad_descriptor.size) {
    gfx::Size content_size = AdSizeToGfxSize(ad_descriptor.size.value());
    config.content_size_.emplace(content_size,
                                 VisibilityToEmbedder::kTransparent,
                                 VisibilityToContent::kTransparent);
  }
  config.deprecated_should_freeze_initial_size_.emplace(
      !ad_descriptor.size.has_value(), VisibilityToEmbedder::kTransparent,
      VisibilityToContent::kOpaque);
  config.ad_auction_data_.emplace(ad_auction_data,
                                  VisibilityToEmbedder::kOpaque,
                                  VisibilityToContent::kOpaque);
  config.on_navigate_callback_ = std::move(on_navigate_callback);

  config.effective_enabled_permissions_ =
      std::vector<network::mojom::PermissionsPolicyFeature>(
          std::begin(network::kFencedFrameFledgeDefaultRequiredFeatures),
          std::end(network::kFencedFrameFledgeDefaultRequiredFeatures));

  AdComponentMetrics metrics(ad_component_descriptors.size());
  std::vector<FencedFrameConfig> nested_configs;
  nested_configs.reserve(ad_component_descriptors.size());
  for (const auto& ad_component_descriptor : ad_component_descriptors) {
    // This config has no urn:uuid. It will later be set when being read into
    // `nested_urn_config_pairs` in `GenerateURNConfigVectorForConfigs()`.
    // For an ad component, the `fenced_frame_reporter` from its parent fenced
    // frame is reused. The pointer to its parent's fenced frame reporter is
    // copied to each ad component. This has the advantage that we do not need
    // to traverse to its parent every time we need its parent's reporter.
    // TODO(crbug.com/40258855): Once the representation of size in fenced frame
    // config is finalized, pass the ad component size from the winning bid to
    // its fenced frame config.
    if (ad_component_descriptor.size) {
      gfx::Size component_content_size =
          AdSizeToGfxSize(ad_component_descriptor.size.value());
      nested_configs.emplace_back(
          /*mapped_url=*/SubstituteSizeIntoURL(ad_component_descriptor),
          /*content_size=*/component_content_size,
          /*fenced_frame_reporter=*/fenced_frame_reporter,
          /*is_ad_component=*/true);
    } else {
      nested_configs.emplace_back(
          /*mapped_url=*/SubstituteSizeIntoURL(ad_component_descriptor),
          /*fenced_frame_reporter=*/fenced_frame_reporter,
          /*is_ad_component=*/true);
    }
    // The ad auction data is added to the nested configs in order to enable
    // leaveAdInterestGroup() for ad components.
    nested_configs.back().ad_auction_data_.emplace(
        ad_auction_data, VisibilityToEmbedder::kOpaque,
        VisibilityToContent::kOpaque);

    metrics.UpdateForSite(net::SchemefulSite(ad_component_descriptor.url));
  }

  config.nested_configs_.emplace(std::move(nested_configs),
                                 VisibilityToEmbedder::kOpaque,
                                 VisibilityToContent::kTransparent);

  config.fenced_frame_reporter_ = std::move(fenced_frame_reporter);
  config.mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
  config.allows_information_inflow_ = false;

  // If we have no ad components, then skip logging our component UMA metrics so
  // we don't dilute them. If we do have components, include the top-level ad's
  // site in the metrics, since the top-level ad might be same-site to one or
  // more components as well.
  if (ad_component_descriptors.size() > 0) {
    metrics.UpdateForSite(net::SchemefulSite(ad_descriptor.url));
    metrics.LogSameSiteMetric();
  }

  metrics.LogTotalCountMetric();

  return config.RedactFor(FencedFrameEntity::kEmbedder);
}

std::optional<GURL> FencedFrameURLMapping::GeneratePendingMappedURN() {
  if (IsFull()) {
    return std::nullopt;
  }

  GURL urn_uuid = GenerateUrnUuid();
  CHECK(!IsMapped(urn_uuid));
  CHECK(!IsPendingMapped(urn_uuid));

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

void FencedFrameURLMapping::RemoveObserverForURN(
    const GURL& urn_uuid,
    MappingResultObserver* observer) {
  auto it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  if (it == pending_urn_uuid_to_url_map_.end()) {
    // A harmless race condition may occur that the pending urn to url map has
    // changed out from under the place that is calling this function (so the
    // destructors were already called), so it's empty.
    return;
  }

  auto observer_it = it->second.find(observer);
  if (observer_it == it->second.end()) {
    // Similarly, the observer may not be associated with the urn.
    return;
  }

  it->second.erase(observer_it);
}

std::optional<FencedFrameConfig>
FencedFrameURLMapping::OnSharedStorageURNMappingResultDetermined(
    const GURL& urn_uuid,
    const SharedStorageURNMappingResult& mapping_result) {
  auto pending_it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  CHECK(pending_it != pending_urn_uuid_to_url_map_.end());

  DCHECK(!IsMapped(urn_uuid));

  std::optional<FencedFrameConfig> config = std::nullopt;

  // Only if the resolved URL is fenced-frame-compatible do we:
  //   1.) Add it to `urn_uuid_to_url_map_`
  //   2.) Report it back to any already-queued observers
  // TODO(crbug.com/40223071): Simplify this by making Shared Storage only
  // capable of producing URLs that fenced frames can navigate to.
  if (blink::IsValidFencedFrameURL(mapping_result.mapped_url)) {
    config = FencedFrameConfig(urn_uuid, mapping_result.mapped_url,
                               mapping_result.budget_metadata,
                               std::move(mapping_result.fenced_frame_reporter));
    config->mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
    config->effective_enabled_permissions_ = {
        std::begin(network::kFencedFrameSharedStorageDefaultRequiredFeatures),
        std::end(network::kFencedFrameSharedStorageDefaultRequiredFeatures)};
    config->allows_information_inflow_ = true;

    urn_uuid_to_url_map_.emplace(urn_uuid, *config);
  }

  std::set<raw_ptr<MappingResultObserver>>& observers = pending_it->second;

  std::optional<FencedFrameProperties> properties = std::nullopt;
  auto final_it = urn_uuid_to_url_map_.find(urn_uuid);
  if (final_it != urn_uuid_to_url_map_.end()) {
    properties = FencedFrameProperties(final_it->second);
  }

  for (MappingResultObserver* observer : observers) {
    observer->OnFencedFrameURLMappingComplete(properties);
  }

  pending_urn_uuid_to_url_map_.erase(pending_it);

  return config;
}

SharedStorageBudgetMetadata*
FencedFrameURLMapping::GetSharedStorageBudgetMetadataForTesting(
    const GURL& urn_uuid) {
  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  CHECK(it != urn_uuid_to_url_map_.end());

  if (!it->second.shared_storage_budget_metadata_) {
    return nullptr;
  }

  return &it->second.shared_storage_budget_metadata_->value_;
}

void FencedFrameURLMapping::SubstituteMappedURL(
    const GURL& urn_uuid,
    const std::vector<std::pair<std::string, std::string>>& substitutions) {
  auto it = urn_uuid_to_url_map_.find(urn_uuid);
  if (it == urn_uuid_to_url_map_.end()) {
    return;
  }
  FencedFrameConfig info = it->second;
  if (info.mapped_url_.has_value()) {
    GURL substituted_url = GURL(SubstituteMappedStrings(
        it->second.mapped_url_->GetValueIgnoringVisibility().spec(),
        substitutions));
    if (!substituted_url.is_valid()) {
      return;
    }
    info.mapped_url_->value_ = substituted_url;
  }
  if (info.nested_configs_.has_value()) {
    for (auto& nested_config : info.nested_configs_->value_) {
      GURL substituted_url = GURL(SubstituteMappedStrings(
          nested_config.mapped_url_->GetValueIgnoringVisibility().spec(),
          substitutions));
      if (!substituted_url.is_valid()) {
        return;
      }
      nested_config.mapped_url_->value_ = substituted_url;
    }
  }
  it->second = std::move(info);
}

bool FencedFrameURLMapping::IsMapped(const GURL& urn_uuid) const {
  return base::Contains(urn_uuid_to_url_map_, urn_uuid);
}

bool FencedFrameURLMapping::IsPendingMapped(const GURL& urn_uuid) const {
  return base::Contains(pending_urn_uuid_to_url_map_, urn_uuid);
}

bool FencedFrameURLMapping::IsFull() const {
  return urn_uuid_to_url_map_.size() + pending_urn_uuid_to_url_map_.size() >=
         kMaxUrnMappingSize;
}

}  // namespace content
