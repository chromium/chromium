// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service_factory.h"

#import "base/check_deref.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

// static
SearchEngineChoiceTriggeringServiceFactory*
SearchEngineChoiceTriggeringServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceTriggeringServiceFactory>
      instance;
  return instance.get();
}

// static
SearchEngineChoiceTriggeringService*
SearchEngineChoiceTriggeringServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<SearchEngineChoiceTriggeringService>(
          profile,
          /*create=*/true);
}

SearchEngineChoiceTriggeringServiceFactory::
    SearchEngineChoiceTriggeringServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SearchEngineChoiceTriggeringService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

SearchEngineChoiceTriggeringServiceFactory::
    ~SearchEngineChoiceTriggeringServiceFactory() = default;

std::unique_ptr<KeyedService>
SearchEngineChoiceTriggeringServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK_EQ(profile, profile->GetOriginalProfile());

  search_engines::SearchEngineChoiceService& search_engine_choice_service =
      CHECK_DEREF(
          ios::SearchEngineChoiceServiceFactory::GetForProfile(profile));
  const policy::PolicyService& policy_service =
      CHECK_DEREF(profile->GetPolicyConnector()->GetPolicyService());
  const TemplateURLService& template_url_service =
      CHECK_DEREF(ios::TemplateURLServiceFactory::GetForProfile(profile));
  auto condition = search_engine_choice_service.GetStaticChoiceScreenConditions(
      policy_service, template_url_service);
  search_engine_choice_service.RecordProfileLoadEligibility(condition);
  if (condition !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    return nullptr;
  }

  return std::make_unique<SearchEngineChoiceTriggeringService>(
      CHECK_DEREF(profile->GetPrefs()), policy_service,
      search_engine_choice_service, template_url_service);
}

}  // namespace ios
