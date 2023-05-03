// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"

#include <cstring>
#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/id_type.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace content {

namespace {

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
      NOTREACHED_NORETURN();
  }
}

gfx::Size AdSizeToGfxSize(const blink::AdSize& ad_size) {
  int width_in_pixels = AdSizeToPixels(ad_size.width, ad_size.width_units);
  int height_in_pixels = AdSizeToPixels(ad_size.height, ad_size.height_units);

  return gfx::Size(width_in_pixels, height_in_pixels);
}

// TODO(crbug.com/1420638): Once the representation of size in fenced frame
// config is finalized, change the type of substituted width and height to the
// same.
// Substitute the size macros in ad url with the size from the winning bid.
GURL SubstituteSizeIntoURL(const blink::AdDescriptor& ad_descriptor) {
  if (!ad_descriptor.size) {
    return ad_descriptor.url;
  }

  // Convert dimensions to pixels.
  gfx::Size size = AdSizeToGfxSize(ad_descriptor.size.value());

  return GURL(SubstituteMappedStrings(
      ad_descriptor.url.spec(),
      {std::make_pair("{%AD_WIDTH%}", base::NumberToString(size.width())),
       std::make_pair("{%AD_HEIGHT%}", base::NumberToString(size.height()))}));
}

}  // namespace

FencedFrameURLMapping::FencedFrameURLMapping() : unique_id_(GetNextId()) {
  CHECK(unique_id_);
}

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
    // TODO(crbug.com/1415475): Change this to a CHECK when we remove urn
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

absl::optional<GURL> FencedFrameURLMapping::AddFencedFrameURLForTesting(
    const GURL& url,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  DCHECK(url.is_valid());
  CHECK(blink::IsValidFencedFrameURL(url));

  auto it = AddMappingForUrl(url);

  if (!it.has_value()) {
    // Insertion fails, the number of urn mappings has reached limit.
    return absl::nullopt;
  }

  auto& [urn, config] = *it.value();

  config.fenced_frame_reporter_ = std::move(fenced_frame_reporter);
  config.mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
  config.deprecated_should_freeze_initial_size_.emplace(
      true, VisibilityToEmbedder::kTransparent, VisibilityToContent::kOpaque);
  // We don't know at this point if the test being run needs the FLEDGE or
  // Shared Storage permissions set. To be safe, we set both here.
  config.required_permissions_to_load.insert(
      config.required_permissions_to_load.end(),
      std::begin(blink::kFencedFrameFledgeDefaultRequiredFeatures),
      std::end(blink::kFencedFrameFledgeDefaultRequiredFeatures));
  config.required_permissions_to_load.insert(
      config.required_permissions_to_load.end(),
      std::begin(blink::kFencedFrameSharedStorageDefaultRequiredFeatures),
      std::end(blink::kFencedFrameSharedStorageDefaultRequiredFeatures));
  return urn;
}

absl::optional<FencedFrameURLMapping::UrnUuidToUrlMap::iterator>
FencedFrameURLMapping::AddMappingForUrl(const GURL& url) {
  if (IsFull()) {
    // Number of urn mappings has reached limit, url will not be inserted.
    return absl::nullopt;
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
    absl::optional<blink::AdSize> container_size,
    const blink::AdDescriptor& ad_descriptor,
    AdAuctionData ad_auction_data,
    base::RepeatingClosure on_navigate_callback,
    std::vector<blink::AdDescriptor> ad_component_descriptors,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  // Move pending mapped urn::uuid to `urn_uuid_to_url_map_`.
  // TODO(crbug.com/1422301): Remove the check for whether `urn_uuid` has been
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
  // TODO(crbug.com/1420638): Once the representation of size in fenced frame
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
  config.ad_auction_data_.emplace(std::move(ad_auction_data),
                                  VisibilityToEmbedder::kOpaque,
                                  VisibilityToContent::kOpaque);
  config.on_navigate_callback_ = std::move(on_navigate_callback);

  config.required_permissions_to_load =
      std::vector<blink::mojom::PermissionsPolicyFeature>(
          std::begin(blink::kFencedFrameFledgeDefaultRequiredFeatures),
          std::end(blink::kFencedFrameFledgeDefaultRequiredFeatures));

  std::vector<FencedFrameConfig> nested_configs;
  nested_configs.reserve(ad_component_descriptors.size());
  for (const auto& ad_component_descriptor : ad_component_descriptors) {
    // This config has no urn:uuid. It will later be set when being read into
    // `nested_urn_config_pairs` in `GenerateURNConfigVectorForConfigs()`.
    // For an ad component, the `fenced_frame_reporter` from its parent fenced
    // frame is reused. The pointer to its parent's fenced frame reporter is
    // copied to each ad component. This has the advantage that we do not need
    // to traverse to its parent every time we need its parent's reporter.
    // TODO(crbug.com/1420638): Once the representation of size in fenced frame
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
  }
  config.nested_configs_.emplace(std::move(nested_configs),
                                 VisibilityToEmbedder::kOpaque,
                                 VisibilityToContent::kTransparent);

  config.fenced_frame_reporter_ = std::move(fenced_frame_reporter);
  config.mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;

  return config.RedactFor(FencedFrameEntity::kEmbedder);
}

absl::optional<GURL> FencedFrameURLMapping::GeneratePendingMappedURN() {
  if (IsFull()) {
    return absl::nullopt;
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

absl::optional<FencedFrameConfig>
FencedFrameURLMapping::OnSharedStorageURNMappingResultDetermined(
    const GURL& urn_uuid,
    const SharedStorageURNMappingResult& mapping_result) {
  auto pending_it = pending_urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(pending_it != pending_urn_uuid_to_url_map_.end());

  DCHECK(!IsMapped(urn_uuid));

  absl::optional<FencedFrameConfig> config = absl::nullopt;

  // Only if the resolved URL is fenced-frame-compatible do we:
  //   1.) Add it to `urn_uuid_to_url_map_`
  //   2.) Report it back to any already-queued observers
  // TODO(crbug.com/1318970): Simplify this by making Shared Storage only
  // capable of producing URLs that fenced frames can navigate to.
  if (blink::IsValidFencedFrameURL(mapping_result.mapped_url)) {
    config = FencedFrameConfig(urn_uuid, mapping_result.mapped_url,
                               mapping_result.budget_metadata,
                               std::move(mapping_result.fenced_frame_reporter));
    config->mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
    config->required_permissions_to_load = {
        std::begin(blink::kFencedFrameSharedStorageDefaultRequiredFeatures),
        std::end(blink::kFencedFrameSharedStorageDefaultRequiredFeatures)};

    urn_uuid_to_url_map_.emplace(urn_uuid, *config);
  }

  std::set<raw_ptr<MappingResultObserver>>& observers = pending_it->second;

  absl::optional<FencedFrameProperties> properties = absl::nullopt;
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
  DCHECK(it != urn_uuid_to_url_map_.end());

  if (!it->second.shared_storage_budget_metadata_)
    return nullptr;

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

// static
FencedFrameURLMapping::Id FencedFrameURLMapping::GetNextId() {
  static Id::Generator generator;
  return generator.GenerateNextId();
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
