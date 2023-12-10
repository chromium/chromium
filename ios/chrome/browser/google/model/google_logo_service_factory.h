// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class KeyedService;
class GoogleLogoService;

// Singleton that owns all GoogleLogoServices and associates them with
// ChromeBrowserState.
class GoogleLogoServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static GoogleLogoService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static GoogleLogoServiceFactory* GetInstance();

  GoogleLogoServiceFactory(const GoogleLogoServiceFactory&) = delete;
  GoogleLogoServiceFactory& operator=(const GoogleLogoServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<GoogleLogoServiceFactory>;

  GoogleLogoServiceFactory();
  ~GoogleLogoServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
