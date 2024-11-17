// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class InMemoryURLIndex;
class ProfileIOS;

namespace ios {
// Singleton that owns all InMemoryURLIndexs and associates them with
// profiles.
class InMemoryURLIndexFactory : public BrowserStateKeyedServiceFactory {
 public:
  static InMemoryURLIndex* GetForProfile(ProfileIOS* profile);
  static InMemoryURLIndexFactory* GetInstance();

  // Returns the default factory used to build InMemoryURLIndexs. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  InMemoryURLIndexFactory(const InMemoryURLIndexFactory&) = delete;
  InMemoryURLIndexFactory& operator=(const InMemoryURLIndexFactory&) = delete;

 private:
  friend class base::NoDestructor<InMemoryURLIndexFactory>;

  InMemoryURLIndexFactory();
  ~InMemoryURLIndexFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_
