// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace autofill {

class StrikeDatabase;

// Singleton that owns all StrikeDatabases and associates them with
// ChromeBrowserState.
class StrikeDatabaseFactory : public BrowserStateKeyedServiceFactory {
 public:
  static StrikeDatabase* GetForBrowserState(ChromeBrowserState* browser_state);
  static StrikeDatabaseFactory* GetInstance();

  StrikeDatabaseFactory(const StrikeDatabaseFactory&) = delete;
  StrikeDatabaseFactory& operator=(const StrikeDatabaseFactory&) = delete;

 private:
  friend class base::NoDestructor<StrikeDatabaseFactory>;

  StrikeDatabaseFactory();
  ~StrikeDatabaseFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
