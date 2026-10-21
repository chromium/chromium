// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#import "components/language/core/browser/accept_languages_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// AcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a Profile.
class AcceptLanguagesServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AcceptLanguagesServiceFactory,
          language::AcceptLanguagesService> {
 public:
  explicit AcceptLanguagesServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
