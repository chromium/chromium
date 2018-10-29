// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

class InMemoryURLIndex;

namespace ios {

class ChromeBrowserState;

// Singleton that owns all InMemoryURLIndexs and associates them with
// ios::ChromeBrowserState.
class InMemoryURLIndexFactory : public BrowserStateKeyedServiceFactory {
 public:
  static InMemoryURLIndex* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static InMemoryURLIndexFactory* GetInstance();

  // Returns the default factory used to build InMemoryURLIndexs. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend struct base::DefaultSingletonTraits<InMemoryURLIndexFactory>;

  InMemoryURLIndexFactory();
  ~InMemoryURLIndexFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(InMemoryURLIndexFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_IN_MEMORY_URL_INDEX_FACTORY_H_
