// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/accept_languages_service_factory.h"

#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// AcceptLanguagesServiceForProfile is a thin container for
// AcceptLanguagesService to enable associating it with a Profile.
class AcceptLanguagesServiceForProfile : public KeyedService {
 public:
  explicit AcceptLanguagesServiceForProfile(PrefService* prefs);

  AcceptLanguagesServiceForProfile(const AcceptLanguagesServiceForProfile&) =
      delete;
  AcceptLanguagesServiceForProfile& operator=(
      const AcceptLanguagesServiceForProfile&) = delete;

  ~AcceptLanguagesServiceForProfile() override;

  // Returns the associated AcceptLanguagesService.
  language::AcceptLanguagesService& accept_languages() {
    return accept_languages_;
  }

 private:
  language::AcceptLanguagesService accept_languages_;
};

AcceptLanguagesServiceForProfile::AcceptLanguagesServiceForProfile(
    PrefService* prefs)
    : accept_languages_(prefs, language::prefs::kAcceptLanguages) {}

AcceptLanguagesServiceForProfile::~AcceptLanguagesServiceForProfile() = default;

}  // namespace

// static
AcceptLanguagesServiceFactory* AcceptLanguagesServiceFactory::GetInstance() {
  static base::NoDestructor<AcceptLanguagesServiceFactory> instance;
  return instance.get();
}

// static
language::AcceptLanguagesService* AcceptLanguagesServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  AcceptLanguagesServiceForProfile* service =
      GetInstance()->GetServiceForProfileAs<AcceptLanguagesServiceForProfile>(
          profile, /*create=*/true);
  return &service->accept_languages();
}

AcceptLanguagesServiceFactory::AcceptLanguagesServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AcceptLanguagesServiceForProfile",
                                    ProfileSelection::kRedirectedInIncognito) {}

AcceptLanguagesServiceFactory::~AcceptLanguagesServiceFactory() = default;

std::unique_ptr<KeyedService>
AcceptLanguagesServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<AcceptLanguagesServiceForProfile>(
      profile->GetPrefs());
}
