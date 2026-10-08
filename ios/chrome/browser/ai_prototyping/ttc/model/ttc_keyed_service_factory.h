// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

class TTCKeyedService;

// Factory for `TTCKeyedService`.
class TTCKeyedServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the `TTCKeyedService` for `profile`, creating it if none exists.
  // Returns nullptr if TTC is not enabled or if the profile is incognito.
  static TTCKeyedService* GetForProfile(ProfileIOS* profile);

  // Returns the singleton `TTCKeyedServiceFactory` instance.
  static TTCKeyedServiceFactory* GetInstance();

  TTCKeyedServiceFactory(const TTCKeyedServiceFactory&) = delete;
  TTCKeyedServiceFactory& operator=(const TTCKeyedServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TTCKeyedServiceFactory>;

  // Default constructor initializing factory traits with
  // `ProfileSelection::kNoInstanceInIncognito`.
  TTCKeyedServiceFactory();
  ~TTCKeyedServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_FACTORY_H_
