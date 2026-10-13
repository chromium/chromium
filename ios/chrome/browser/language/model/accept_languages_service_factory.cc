// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/accept_languages_service_factory.h"

#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  static base::NoDestructor<AcceptLanguagesServiceFactory> instance;
  return instance.get();
}

// static
language::AcceptLanguagesService* AcceptLanguagesServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<language::AcceptLanguagesService>(
          profile, /*create=*/true);
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AcceptLanguagesServiceForProfile",
                                    ProfileSelection::kRedirectedInIncognito) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() = default;

std::unique_ptr<KeyedService>
AcceptLanguagesServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<language::AcceptLanguagesService>(
      profile->GetPrefs(), language::prefs::kAcceptLanguages);
}
