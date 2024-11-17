// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

// A factory for creating one `FieldClassificationModelHandler` per browser
// state.
class IOSPasswordFieldClassificationModelHandlerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSPasswordFieldClassificationModelHandlerFactory* GetInstance();
  static autofill::FieldClassificationModelHandler* GetForProfile(
      ProfileIOS* profile);

  IOSPasswordFieldClassificationModelHandlerFactory(
      const IOSPasswordFieldClassificationModelHandlerFactory&) = delete;
  IOSPasswordFieldClassificationModelHandlerFactory& operator=(
      const IOSPasswordFieldClassificationModelHandlerFactory&) = delete;

 private:
  friend base::NoDestructor<IOSPasswordFieldClassificationModelHandlerFactory>;

  IOSPasswordFieldClassificationModelHandlerFactory();
  ~IOSPasswordFieldClassificationModelHandlerFactory() override;

  // BrowserStateKeyedServiceFactory overrides:
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
