// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class DiscoverFeedService;

// Singleton that owns all DiscoverFeedServices and associates them with
// ChromeBrowserState.
class DiscoverFeedServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358299863): Remove when fully migrated.
  static DiscoverFeedService* GetForBrowserState(
      ChromeBrowserState* browser_state,
      bool create = true);

  static DiscoverFeedService* GetForProfile(ProfileIOS* profile,
                                            bool create = true);

  static DiscoverFeedServiceFactory* GetInstance();

  DiscoverFeedServiceFactory(const DiscoverFeedServiceFactory&) = delete;
  DiscoverFeedServiceFactory& operator=(const DiscoverFeedServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<DiscoverFeedServiceFactory>;

  DiscoverFeedServiceFactory();
  ~DiscoverFeedServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_SERVICE_FACTORY_H_
