// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/ios_contextual_tasks_service_factory.h"

#import "base/functional/bind.h"
#import "components/contextual_tasks/internal/composite_context_decorator.h"
#import "components/contextual_tasks/internal/contextual_tasks_service_impl.h"
#import "components/contextual_tasks/public/contextual_tasks_service.h"
#import "components/contextual_tasks/public/features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/sync/base/features.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace {

// Callback used by ContextualTasksService to report the number of active
// tasks for metrics logging (specifically when creating a new task). On iOS,
// we currently assume a model with at most one active task at a time, meaning
// that when a new task is being created, the number of existing active tasks
// is effectively 0.
// TODO(crbug.com/500673669): Document in the component code that some platforms
// assume having at most 1 active task.
size_t GetNumberOfActiveTasks() {
  return 0;
}

// Currently the service is only used for AIM threads.
bool IsGeminiThreadsEnabled() {
  return false;
}

}  // namespace

// static
contextual_tasks::ContextualTasksService*
IOSContextualTasksServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<contextual_tasks::ContextualTasksService>(
          profile, /*create=*/true);
}

// static
IOSContextualTasksServiceFactory*
IOSContextualTasksServiceFactory::GetInstance() {
  static base::NoDestructor<IOSContextualTasksServiceFactory> instance;
  return instance.get();
}

IOSContextualTasksServiceFactory::IOSContextualTasksServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ContextualTasksService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(IOSChromeAimEligibilityServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSContextualTasksServiceFactory::~IOSContextualTasksServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSContextualTasksServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  favicon::FaviconService* favicon_service =
      ios::FaviconServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  bool supports_ephemeral_only = profile->IsOffTheRecord();

  return std::make_unique<contextual_tasks::ContextualTasksServiceImpl>(
      ::GetChannel(),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      contextual_tasks::CreateCompositeContextDecorator(favicon_service,
                                                        history_service, {}),
      aim_eligibility_service, identity_manager, profile->GetPrefs(),
      supports_ephemeral_only, base::BindRepeating(&GetNumberOfActiveTasks),
      base::BindRepeating(&IsGeminiThreadsEnabled));
}
