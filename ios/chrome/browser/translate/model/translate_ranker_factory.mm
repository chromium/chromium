// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_ranker_factory.h"

#import "base/no_destructor.h"
#import "components/translate/core/browser/translate_ranker_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace translate {

// static
TranslateRankerFactory* TranslateRankerFactory::GetInstance() {
  static base::NoDestructor<TranslateRankerFactory> instance;
  return instance.get();
}

// static
translate::TranslateRanker* TranslateRankerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<translate::TranslateRanker>(
      profile, /*create=*/true);
}

TranslateRankerFactory::TranslateRankerFactory()
    : ProfileKeyedServiceFactoryIOS("TranslateRankerService",
                                    ProfileSelection::kRedirectedInIncognito) {}

TranslateRankerFactory::~TranslateRankerFactory() {}

std::unique_ptr<KeyedService> TranslateRankerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<TranslateRankerImpl>(
      TranslateRankerImpl::GetModelPath(profile->GetStatePath()),
      TranslateRankerImpl::GetModelURL(),
      GetApplicationContext()->GetUkmRecorder());
}

}  // namespace translate
