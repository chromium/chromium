// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_content_annotations/model/page_content_annotations_service_factory.h"

#import "base/feature_list.h"
#import "base/path_service.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/page_content_annotations/core/page_content_annotations_features.h"
#import "components/page_content_annotations/core/page_content_annotations_service.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildPageContentAnnotationsService(
    ProfileIOS* profile) {
  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());
  if (!page_content_annotations::features::
          ShouldEnablePageContentAnnotations()) {
    return nullptr;
  }

  // The optimization guide and history services must be available for the page
  // content annotations service to work.
  OptimizationGuideService* optimization_guide_keyed_service =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!optimization_guide_keyed_service || !history_service) {
    return nullptr;
  }

  leveldb_proto::ProtoDatabaseProvider* proto_db_provider =
      profile->GetProtoDatabaseProvider();
  base::FilePath profile_path = profile->GetOriginalProfile()->GetStatePath();

  return std::make_unique<
      page_content_annotations::PageContentAnnotationsService>(
      GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
      GetCurrentCountryCode(GetApplicationContext()->GetVariationsService()),
      optimization_guide_keyed_service, history_service,
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      ios::ZeroSuggestCacheServiceFactory::GetForProfile(profile),
      proto_db_provider, profile_path,
      optimization_guide_keyed_service->GetOptimizationGuideLogger(),
      optimization_guide_keyed_service,
      /*embedder_metadata_provider=*/nullptr,
      /*embedder=*/nullptr,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

}  // namespace

// static
page_content_annotations::PageContentAnnotationsService*
PageContentAnnotationsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          page_content_annotations::PageContentAnnotationsService>(
          profile, /*create=*/true);
}

// static
PageContentAnnotationsServiceFactory*
PageContentAnnotationsServiceFactory::GetInstance() {
  static base::NoDestructor<PageContentAnnotationsServiceFactory> instance;
  return instance.get();
}

PageContentAnnotationsServiceFactory::PageContentAnnotationsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PageContentAnnotationsService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::ZeroSuggestCacheServiceFactory::GetInstance());
}

PageContentAnnotationsServiceFactory::~PageContentAnnotationsServiceFactory() =
    default;

// static
PageContentAnnotationsServiceFactory::TestingFactory
PageContentAnnotationsServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildPageContentAnnotationsService);
}

std::unique_ptr<KeyedService>
PageContentAnnotationsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildPageContentAnnotationsService(profile);
}
