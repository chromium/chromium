// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_

#import "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A factory for creating one `FieldClassificationModelHandler` per profile.
class IOSPasswordFieldClassificationModelHandlerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSPasswordFieldClassificationModelHandlerFactory,
          autofill::FieldClassificationModelHandler> {
 public:
  explicit IOSPasswordFieldClassificationModelHandlerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
