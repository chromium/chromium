// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ScreenTimeHistoryDeleter;

// Factory that owns and associates a ScreenTimeHistoryDeleter with
// ChromeBrowserState.
class API_AVAILABLE(ios(14.0)) ScreenTimeHistoryDeleterFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static ScreenTimeHistoryDeleter* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static ScreenTimeHistoryDeleterFactory* GetInstance();

  ScreenTimeHistoryDeleterFactory(const ScreenTimeHistoryDeleterFactory&) =
      delete;
  ScreenTimeHistoryDeleterFactory& operator=(
      const ScreenTimeHistoryDeleterFactory&) = delete;

 private:
  friend class base::NoDestructor<ScreenTimeHistoryDeleterFactory>;

  ScreenTimeHistoryDeleterFactory();
  ~ScreenTimeHistoryDeleterFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
