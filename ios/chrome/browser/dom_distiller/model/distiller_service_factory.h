// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

// Singleton that creates the Distiller service for a given profile.
class DistillerServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static DistillerService* GetForProfile(ProfileIOS* profile);
  static DistillerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<DistillerServiceFactory>;

  DistillerServiceFactory();
  ~DistillerServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_
