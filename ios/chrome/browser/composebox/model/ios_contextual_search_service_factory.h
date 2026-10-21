// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_

#import "components/contextual_search/contextual_search_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all ContextualSearchService and associates them
// with ProfileIOS.
class ContextualSearchServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ContextualSearchServiceFactory,
          contextual_search::ContextualSearchService> {
 public:
  explicit ContextualSearchServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_FACTORY_H_
