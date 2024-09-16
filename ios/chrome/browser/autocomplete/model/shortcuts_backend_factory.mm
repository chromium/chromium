// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/omnibox/browser/shortcuts_constants.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/search_engines/model/ui_thread_search_terms_data.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {
namespace {

scoped_refptr<ShortcutsBackend> CreateShortcutsBackend(ProfileIOS* profile,
                                                       bool suppress_db) {
  scoped_refptr<ShortcutsBackend> shortcuts_backend(new ShortcutsBackend(
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      std::make_unique<ios::UIThreadSearchTermsData>(),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetStatePath().Append(kShortcutsDatabaseName), suppress_db));
  return shortcuts_backend->Init() ? shortcuts_backend : nullptr;
}

scoped_refptr<RefcountedKeyedService> BuildShortcutsBackend(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return CreateShortcutsBackend(profile, false /* suppress_db */);
}

}  // namespace

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfile(
    ProfileIOS* profile) {
  return base::WrapRefCounted(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(profile, true).get()));
}

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return base::WrapRefCounted(static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserState(profile, false).get()));
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  static base::NoDestructor<ShortcutsBackendFactory> instance;
  return instance.get();
}

// static
ShortcutsBackendFactory::TestingFactory
ShortcutsBackendFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildShortcutsBackend);
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "ShortcutsBackend",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() {}

scoped_refptr<RefcountedKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildShortcutsBackend(context);
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
