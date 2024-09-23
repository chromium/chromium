// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/visited_url_ranking/model/visited_url_ranking_service_factory.h"

#import "components/history_clusters/core/config.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/visited_url_ranking/internal/history_url_visit_data_fetcher.h"
#import "components/visited_url_ranking/internal/session_url_visit_data_fetcher.h"
#import "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"
#import "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_categories_transformer.h"
#import "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_visibility_score_transformer.h"
#import "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"
#import "components/visited_url_ranking/internal/transformer/url_visit_aggregates_segmentation_metrics_transformer.h"
#import "components/visited_url_ranking/internal/visited_url_ranking_service_impl.h"
#import "components/visited_url_ranking/public/url_visit_util.h"
#import "components/visited_url_ranking/public/visited_url_ranking_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/visited_url_ranking/model/ios_tab_model_url_visit_data_fetcher.h"

// static
visited_url_ranking::VisitedURLRankingService*
VisitedURLRankingServiceFactory::GetForProfile(ProfileIOS* state) {
  return static_cast<visited_url_ranking::VisitedURLRankingService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

VisitedURLRankingServiceFactory::VisitedURLRankingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "VisitedURLRanking",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

// static
VisitedURLRankingServiceFactory*
VisitedURLRankingServiceFactory::GetInstance() {
  static base::NoDestructor<VisitedURLRankingServiceFactory> instance;
  return instance.get();
}

VisitedURLRankingServiceFactory::~VisitedURLRankingServiceFactory() {}

std::unique_ptr<KeyedService>
VisitedURLRankingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::map<visited_url_ranking::Fetcher,
           std::unique_ptr<visited_url_ranking::URLVisitDataFetcher>>
      data_fetchers;

  data_fetchers.emplace(
      visited_url_ranking::Fetcher::kTabModel,
      std::make_unique<visited_url_ranking::IOSTabModelURLVisitDataFetcher>(
          profile));

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetForProfile(profile);
  if (session_sync_service) {
    data_fetchers.emplace(
        visited_url_ranking::Fetcher::kSession,
        std::make_unique<visited_url_ranking::SessionURLVisitDataFetcher>(
            session_sync_service));
  }
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service) {
    syncer::DeviceInfoSyncService* device_info_sync_service =
        DeviceInfoSyncServiceFactory::GetForProfile(profile);
    data_fetchers.emplace(
        visited_url_ranking::Fetcher::kHistory,
        std::make_unique<visited_url_ranking::HistoryURLVisitDataFetcher>(
            history_service, device_info_sync_service));
  }

  std::map<visited_url_ranking::URLVisitAggregatesTransformType,
           std::unique_ptr<visited_url_ranking::URLVisitAggregatesTransformer>>
      transformers = {};

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  transformers.emplace(
      visited_url_ranking::URLVisitAggregatesTransformType::
          kSegmentationMetricsData,
      std::make_unique<visited_url_ranking::
                           URLVisitAggregatesSegmentationMetricsTransformer>(
          segmentation_platform_service));

  // TODO(crbug.com/329242209): Add various aggregate transformers (e.g,
  // shopping) to the service's map of supported transformers.
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForProfile(profile);
  if (bookmark_model) {
    auto bookmarks_transformer = std::make_unique<
        visited_url_ranking::BookmarksURLVisitAggregatesTransformer>(
        bookmark_model);
    transformers.emplace(
        visited_url_ranking::URLVisitAggregatesTransformType::kBookmarkData,
        std::move(bookmarks_transformer));
  }

  transformers.emplace(
      visited_url_ranking::URLVisitAggregatesTransformType::
          kHistoryVisibilityScoreFilter,
      std::make_unique<visited_url_ranking::
                           HistoryURLVisitAggregatesVisibilityScoreTransformer>(
          history_clusters::Config().content_visibility_threshold));
  transformers.emplace(
      visited_url_ranking::URLVisitAggregatesTransformType::
          kHistoryCategoriesFilter,
      std::make_unique<
          visited_url_ranking::HistoryURLVisitAggregatesCategoriesTransformer>(
          base::flat_set<std::string>(
              visited_url_ranking::kBlocklistedCategories.begin(),
              visited_url_ranking::kBlocklistedCategories.end())));

  transformers.emplace(
      visited_url_ranking::URLVisitAggregatesTransformType::kRecencyFilter,
      std::make_unique<visited_url_ranking::RecencyFilterTransformer>());

  return std::make_unique<visited_url_ranking::VisitedURLRankingServiceImpl>(
      segmentation_platform_service, std::move(data_fetchers),
      std::move(transformers));
}
