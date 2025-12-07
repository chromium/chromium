// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace translate {

class TranslateRanker;

// TranslateRankerFactory is a way to associate a TranslateRanker instance to
// a Profile.
class TranslateRankerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static translate::TranslateRanker* GetForProfile(ProfileIOS* profile);
  static TranslateRankerFactory* GetInstance();

 private:
  friend class base::NoDestructor<TranslateRankerFactory>;

  TranslateRankerFactory();
  ~TranslateRankerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace translate

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
