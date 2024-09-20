// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;
class GoogleLogoService;

// Singleton that owns all GoogleLogoServices and associates them with
// profiles.
class GoogleLogoServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static GoogleLogoService* GetForBrowserState(ProfileIOS* profile);

  static GoogleLogoService* GetForProfile(ProfileIOS* profile);
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
