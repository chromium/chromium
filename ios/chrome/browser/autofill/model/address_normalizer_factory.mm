// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/address_normalizer_factory.h"

#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/autofill/model/validation_rules_storage_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#import "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {
namespace {

std::unique_ptr<::i18n::addressinput::Source> GetAddressInputSource() {
  return base::WrapUnique(new autofill::ChromeMetadataSource(
      I18N_ADDRESS_VALIDATION_DATA_URL,
      GetApplicationContext()->GetSharedURLLoaderFactory()));
}

std::unique_ptr<::i18n::addressinput::Storage> GetAddressInputStorage() {
  return autofill::ValidationRulesStorageFactory::CreateStorage();
}

}  // namespace

// static
AddressNormalizer* AddressNormalizerFactory::GetInstance() {
  static base::NoDestructor<AddressNormalizerFactory> instance;
  return &(instance->address_normalizer_);
}

AddressNormalizerFactory::AddressNormalizerFactory()
    : address_normalizer_(GetAddressInputSource(),
                          GetAddressInputStorage(),
                          GetApplicationContext()->GetApplicationLocale()) {}

AddressNormalizerFactory::~AddressNormalizerFactory() {}

}  // namespace autofill
