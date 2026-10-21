// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_

#import <memory>

#import "components/translate/core/browser/translate_ranker.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace translate {

// TranslateRankerFactory is a way to associate a TranslateRanker instance to
// a Profile.
class TranslateRankerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TranslateRankerFactory,
                                                translate::TranslateRanker> {
 public:
  explicit TranslateRankerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace translate

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
