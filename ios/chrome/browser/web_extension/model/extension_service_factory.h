// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"

// Factory for creating `ExtensionService` for a profile on iOS.
class ExtensionServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ExtensionServiceFactory,
                                                ExtensionService> {
 public:
  explicit ExtensionServiceFactory(PassKey key);

  // Returns the default factory used to build `ExtensionService`.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_FACTORY_H_
