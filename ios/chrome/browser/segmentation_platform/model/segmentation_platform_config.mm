// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_config.h"

#import <memory>
#import <string_view>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"
#import "components/segmentation_platform/embedder/default_model/cross_device_user_segment.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/feed_user_segment.h"
#import "components/segmentation_platform/embedder/default_model/ios_module_ranker.h"
#import "components/segmentation_platform/embedder/default_model/low_user_engagement_model.h"
#import "components/segmentation_platform/embedder/default_model/most_visited_tiles_user.h"
#import "components/segmentation_platform/embedder/default_model/password_manager_user_segment.h"
#import "components/segmentation_platform/embedder/default_model/search_user_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/embedder/default_model/tab_resumption_ranker.h"
#import "components/segmentation_platform/embedder/default_model/url_visit_resumption_ranker.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"
#import "components/segmentation_platform/internal/stats.h"
#import "components/segmentation_platform/public/config.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/model_provider.h"
#import "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"

namespace segmentation_platform {

namespace {

using ::segmentation_platform::proto::SegmentId;

}  // namespace

using proto::SegmentId;

std::vector<std::unique_ptr<Config>> GetSegmentationPlatformConfig(
    home_modules::HomeModulesCardRegistry* homeModulesCardRegistry) {
  std::vector<std::unique_ptr<Config>> configs;
  configs.emplace_back(FeedUserSegment::GetConfig());
  configs.emplace_back(CrossDeviceUserSegment::GetConfig());
  configs.emplace_back(SearchUserModel::GetConfig());
  configs.emplace_back(DeviceSwitcherModel::GetConfig());
  configs.emplace_back(LowUserEngagementModel::GetConfig());
  configs.emplace_back(TabResumptionRanker::GetConfig());
  configs.emplace_back(PasswordManagerUserModel::GetConfig());
  configs.emplace_back(ShoppingUserModel::GetConfig());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "test-ios-module-ranker")) {
    configs.emplace_back(TestIosModuleRanker::GetConfig());
  } else {
    configs.emplace_back(IosModuleRanker::GetConfig());
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEphemeralModuleBackendRankerTestOverride)) {
    configs.emplace_back(
        home_modules::TestEphemeralHomeModuleBackend::GetConfig());
  } else {
    configs.emplace_back(home_modules::EphemeralHomeModuleBackend::GetConfig(
        homeModulesCardRegistry));
  }
  configs.emplace_back(MostVisitedTilesUser::GetConfig());
  configs.emplace_back(URLVisitResumptionRanker::GetConfig());

  // Add new configs here.
  std::erase_if(configs, [](const auto& config) { return !config.get(); });
  return configs;
}

IOSFieldTrialRegisterImpl::IOSFieldTrialRegisterImpl() = default;
IOSFieldTrialRegisterImpl::~IOSFieldTrialRegisterImpl() = default;

void IOSFieldTrialRegisterImpl::RegisterFieldTrial(
    std::string_view trial_name,
    std::string_view group_name) {
  // See this comment for limitations of using this API:
  // chrome/browser/segmentation_platform/segmentation_platform_config.cc.
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

void IOSFieldTrialRegisterImpl::RegisterSubsegmentFieldTrialIfNeeded(
    std::string_view trial_name,
    SegmentId segment_id,
    int subsegment_rank) {
  // Per target checks should be replaced by making this as a ModelProvider
  // method.
  std::optional<std::string> group_name;

  if (!group_name) {
    return;
  }
  RegisterFieldTrial(trial_name, *group_name);
}

}  // namespace segmentation_platform
