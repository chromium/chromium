// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/autofill_profile_validator_factory.h"

#include <memory>

#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

AutofillProfileValidator* AutofillProfileValidatorFactory::GetInstance() {
  static base::LazyInstance<AutofillProfileValidatorFactory>::DestructorAtExit
      instance = LAZY_INSTANCE_INITIALIZER;
  return &(instance.Get().autofill_profile_validator_);
}

AutofillProfileValidatorFactory::AutofillProfileValidatorFactory()
    : autofill_profile_validator_(
          std::make_unique<autofill::ChromeMetadataSource>(
              I18N_ADDRESS_VALIDATION_DATA_URL,
              GetApplicationContext()->GetSharedURLLoaderFactory()),
          ValidationRulesStorageFactory::CreateStorage()) {}

AutofillProfileValidatorFactory::~AutofillProfileValidatorFactory() {}

}  // namespace autofill
