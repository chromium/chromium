// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"

#import <memory>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"
#import "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
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

using ::segmentation_platform::proto::SegmentId;

constexpr int kFeedUserSegmentSelectionTTLDays = 14;
constexpr int kFeedUserSegmentUnknownSelectionTTLDays = 14;

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

}  // namespace

using proto::SegmentId;

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig() {
  std::vector<std::unique_ptr<Config>> configs;
  if (base::FeatureList::IsEnabled(
          features::kSegmentationPlatformFeedSegmentFeature)) {
    configs.emplace_back(GetConfigForFeedSegments());
  }

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

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
