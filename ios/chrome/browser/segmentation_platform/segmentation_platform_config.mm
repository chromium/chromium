// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/segmentation_platform/segmentation_platform_config.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"

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
  config->segment_ids = {
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
  };
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

std::unique_ptr<ModelProvider> GetDefaultModelProvider(
    proto::SegmentId target) {
  // Add default models here.
  return nullptr;
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
    int subsegment_rank) {}

}  // namespace segmentation_platform
