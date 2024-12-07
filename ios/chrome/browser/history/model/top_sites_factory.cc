// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/model/top_sites_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/history/model/history_utils.h"
#include "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<history::TopSites>(
      profile, /*create=*/true);
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  static base::NoDestructor<TopSitesFactory> instance;
  return instance.get();
}

TopSitesFactory::TopSitesFactory()
    : RefcountedProfileKeyedServiceFactoryIOS(
          "TopSites",
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

TopSitesFactory::~TopSitesFactory() = default;

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto top_sites = base::MakeRefCounted<history::TopSitesImpl>(
      profile->GetPrefs(), history_service,
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      history::PrepopulatedPageList(), base::BindRepeating(CanAddURLToHistory));
  top_sites->Init(profile->GetStatePath().Append(history::kTopSitesFilename));
  return top_sites;
}

void TopSitesFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  history::TopSitesImpl::RegisterPrefs(registry);
}

}  // namespace ios
