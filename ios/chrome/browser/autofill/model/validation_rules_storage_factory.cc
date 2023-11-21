// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/model/validation_rules_storage_factory.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "components/prefs/json_pref_store.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"

namespace autofill {

using ::i18n::addressinput::Storage;

// static
std::unique_ptr<Storage> ValidationRulesStorageFactory::CreateStorage() {
  static base::NoDestructor<ValidationRulesStorageFactory> instance;
  return std::unique_ptr<Storage>(
      new ChromeStorageImpl(instance->json_pref_store_.get()));
}

ValidationRulesStorageFactory::ValidationRulesStorageFactory() {
  base::FilePath user_data_dir;
  bool success = base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
  DCHECK(success);

  json_pref_store_ = new JsonPrefStore(
      user_data_dir.Append(FILE_PATH_LITERAL("Address Validation Rules")));
  json_pref_store_->ReadPrefsAsync(NULL);
}

ValidationRulesStorageFactory::~ValidationRulesStorageFactory() {}

}  // namespace autofill
