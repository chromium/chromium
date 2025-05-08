// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace autofill {
class FieldClassificationModelHandler;
}

// A factory for creating one `FieldClassificationModelHandler` per profile.
class IOSAutofillFieldClassificationModelHandlerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSAutofillFieldClassificationModelHandlerFactory* GetInstance();
  static autofill::FieldClassificationModelHandler* GetForProfile(
      ProfileIOS* profile);

 private:
  friend base::NoDestructor<IOSAutofillFieldClassificationModelHandlerFactory>;

  IOSAutofillFieldClassificationModelHandlerFactory();
  ~IOSAutofillFieldClassificationModelHandlerFactory() override;

  // BrowserStateKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
