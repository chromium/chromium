// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_

#import "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A factory for creating one `FieldClassificationModelHandler` per profile.
class IOSAutofillFieldClassificationModelHandlerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAutofillFieldClassificationModelHandlerFactory,
          autofill::FieldClassificationModelHandler> {
 public:
  explicit IOSAutofillFieldClassificationModelHandlerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_IOS_AUTOFILL_FIELD_CLASSIFICATION_MODEL_HANDLER_FACTORY_H_
