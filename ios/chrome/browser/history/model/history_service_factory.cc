// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/history_service_factory.h"

#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/ios/browser/history_database_helper.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#include "ios/chrome/browser/history/model/history_client_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/common/channel_info.h"

namespace ios {

namespace {

std::unique_ptr<HistoryClientImpl> BuildHistoryClient(ProfileIOS* profile) {
  return std::make_unique<HistoryClientImpl>(
      BookmarkModelFactory::GetForProfile(profile));
}

std::unique_ptr<KeyedService> BuildHistoryService(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<history::HistoryService> history_service(
      new history::HistoryService(BuildHistoryClient(profile), nullptr));
  if (!history_service->Init(history::HistoryDatabaseParamsForPath(
          profile->GetStatePath(), GetChannel()))) {
    return nullptr;
  }
  return history_service;
}

}  // namespace

// static
history::HistoryService* HistoryServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  // If saving history is disabled, only allow explicit access.
  if (access_type != ServiceAccessType::EXPLICIT_ACCESS &&
      profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return nullptr;
  }

  return GetInstance()->GetServiceForProfileAs<history::HistoryService>(
      profile, /*create=*/true);
}

// static
HistoryServiceFactory* HistoryServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryServiceFactory> instance;
  return instance.get();
}

// static
HistoryServiceFactory::TestingFactory
HistoryServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildHistoryService);
}

HistoryServiceFactory::HistoryServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HistoryService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

HistoryServiceFactory::~HistoryServiceFactory() = default;

std::unique_ptr<KeyedService> HistoryServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildHistoryService(context);
}

}  // namespace ios
