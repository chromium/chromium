// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_

#import <memory>

#import "components/visited_url_ranking/public/visited_url_ranking_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for the components VisitedURLRankingService service which fetches and
// ranks visited URL.
class VisitedURLRankingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          VisitedURLRankingServiceFactory,
          visited_url_ranking::VisitedURLRankingService> {
 public:
  explicit VisitedURLRankingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_VISITED_URL_RANKING_SERVICE_FACTORY_H_
