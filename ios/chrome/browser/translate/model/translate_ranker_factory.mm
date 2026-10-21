// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_ranker_factory.h"

#import <utility>

#import "components/translate/core/browser/translate_ranker_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace translate {

TranslateRankerFactory::TranslateRankerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "TranslateRankerService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService> TranslateRankerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<TranslateRankerImpl>(
      TranslateRankerImpl::GetModelPath(profile->GetStatePath()),
      TranslateRankerImpl::GetModelURL(),
      GetApplicationContext()->GetUkmRecorder());
}

}  // namespace translate
