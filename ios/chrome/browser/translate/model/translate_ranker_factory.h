// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace translate {

class TranslateRanker;

// TranslateRankerFactory is a way to associate a TranslateRanker instance to
// a Profile.
class TranslateRankerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static translate::TranslateRanker* GetForProfile(ProfileIOS* profile);
  static TranslateRankerFactory* GetInstance();

  TranslateRankerFactory(const TranslateRankerFactory&) = delete;
  TranslateRankerFactory& operator=(const TranslateRankerFactory&) = delete;

 private:
  friend class base::NoDestructor<TranslateRankerFactory>;

  TranslateRankerFactory();
  ~TranslateRankerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace translate

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_RANKER_FACTORY_H_
