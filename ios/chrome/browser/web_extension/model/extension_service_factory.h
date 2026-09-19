// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ExtensionService;
class ProfileIOS;

// Factory for creating `ExtensionService` for a profile on iOS.
class ExtensionServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the `ExtensionService` for `profile`, creating one if needed.
  static ExtensionService* GetForProfile(ProfileIOS* profile);

  // Returns the singleton instance of `ExtensionServiceFactory`.
  static ExtensionServiceFactory* GetInstance();

  // Returns the default factory used to build `ExtensionService`.
  static TestingFactory GetDefaultFactory();

  ExtensionServiceFactory(const ExtensionServiceFactory&) = delete;
  ExtensionServiceFactory& operator=(const ExtensionServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ExtensionServiceFactory>;

  ExtensionServiceFactory();
  ~ExtensionServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_
