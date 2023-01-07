// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"

#import <memory>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"
#import "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"
#import "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#import "components/segmentation_platform/embedder/default_model/search_user_model.h"
#import "components/segmentation_platform/internal/stats.h"
#import "components/segmentation_platform/public/config.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/model_provider.h"
#import "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace segmentation_platform {

namespace {

constexpr char kDefaultModelEnabledParam[] = "enable_default_model";

using ::segmentation_platform::proto::SegmentId;

constexpr int kFeedUserSegmentSelectionTTLDays = 14;
constexpr int kFeedUserSegmentUnknownSelectionTTLDays = 14;

constexpr int kCrossDeviceUserSegmentSelectionTTLDays = 7;
constexpr int kCrossDeviceUserSegmentUnknownSelectionTTLDays = 7;

constexpr int kSearchUserSegmentSelectionTTLDays = 7;
constexpr int kSearchUserSegmentUnknownSelectionTTLDays = 7;

std::unique_ptr<Config> GetConfigForFeedSegments() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kFeedUserSegmentationKey;
  config->segmentation_uma_name = kFeedUserSegmentUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER);
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          "segment_selection_ttl_days", kFeedUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformFeedSegmentFeature,
          "unknown_selection_ttl_days",
          kFeedUserSegmentUnknownSelectionTTLDays));
  return config;
}

std::unique_ptr<Config> GetConfigForCrossDeviceSegments() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kCrossDeviceUserKey;
  config->segmentation_uma_name = kCrossDeviceUserUmaName;
  config->AddSegmentId(SegmentId::CROSS_DEVICE_USER_SEGMENT,
                       std::make_unique<CrossDeviceUserSegment>());
  config->segment_selection_ttl =
      base::Days(kCrossDeviceUserSegmentSelectionTTLDays);
  config->unknown_selection_ttl =
      base::Days(kCrossDeviceUserSegmentUnknownSelectionTTLDays);
  return config;
}

std::unique_ptr<ModelProvider> GetSearchUserDefaultModel() {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kSegmentationPlatformSearchUser, kDefaultModelEnabledParam,
          true)) {
    return nullptr;
  }
  return std::make_unique<SearchUserModel>();
}

std::unique_ptr<Config> GetConfigForSearchUserModel() {
  auto config = std::make_unique<Config>();
  config->segmentation_key = kSearchUserKey;
  config->segmentation_uma_name = kSearchUserUmaName;
  config->AddSegmentId(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER,
                       GetSearchUserDefaultModel());
  config->segment_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformSearchUser,
          "segment_selection_ttl_days", kSearchUserSegmentSelectionTTLDays));
  config->unknown_selection_ttl =
      base::Days(base::GetFieldTrialParamByFeatureAsInt(
          features::kSegmentationPlatformSearchUser,
          "unknown_selection_ttl_days",
          kSearchUserSegmentUnknownSelectionTTLDays));
  return config;
}

}  // namespace

using proto::SegmentId;

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig() {
  std::vector<std::unique_ptr<Config>> configs;
  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    configs.emplace_back(GetConfigForFeedSegments());
  }
  if (base::FeatureList::IsEnabled(features::kSegmentationPlatformSearchUser)) {
    configs.emplace_back(GetConfigForSearchUserModel());
  }

  configs.emplace_back(GetConfigForCrossDeviceSegments());

  // Add new configs here.

  return configs;
}

IOSFieldTrialRegisterImpl::IOSFieldTrialRegisterImpl() = default;
IOSFieldTrialRegisterImpl::~IOSFieldTrialRegisterImpl() = default;

void IOSFieldTrialRegisterImpl::RegisterFieldTrial(
    base::StringPiece trial_name,
    base::StringPiece group_name) {
  // See this comment for limitations of using this API:
  // chrome/browser/segmentation_platform/segmentation_platform_config.cc.
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      std::string(trial_name), std::string(group_name),
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

void IOSFieldTrialRegisterImpl::RegisterSubsegmentFieldTrialIfNeeded(
    base::StringPiece trial_name,
    SegmentId segment_id,
    int subsegment_rank) {
  // Per target checks should be replaced by making this as a ModelProvider
  // method.
  absl::optional<std::string> group_name;
  if (segment_id == SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER) {
    group_name = FeedUserSegment::GetSubsegmentName(subsegment_rank);
  }
  if (segment_id == SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SEARCH_USER) {
    group_name = SearchUserModel::GetSubsegmentName(subsegment_rank);
  }

  if (segment_id == SegmentId::CROSS_DEVICE_USER_SEGMENT) {
    group_name = CrossDeviceUserSegment::GetSubsegmentName(subsegment_rank);
  }

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
