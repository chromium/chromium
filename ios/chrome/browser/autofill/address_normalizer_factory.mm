// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/address_normalizer_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
