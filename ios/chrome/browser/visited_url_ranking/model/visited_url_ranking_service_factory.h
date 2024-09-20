// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace visited_url_ranking {
class VisitedURLRankingService;
}

// Factory for the components VisitedURLRankingService service which fetches and
// ranks visited URL.
class VisitedURLRankingServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static visited_url_ranking::VisitedURLRankingService* GetForProfile(
      ProfileIOS* profile);
  static VisitedURLRankingServiceFactory* GetInstance();

  VisitedURLRankingServiceFactory(const VisitedURLRankingServiceFactory&) =
      delete;
  VisitedURLRankingServiceFactory& operator=(
      const VisitedURLRankingServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<VisitedURLRankingServiceFactory>;

  VisitedURLRankingServiceFactory();
  ~VisitedURLRankingServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_
