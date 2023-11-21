// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_VALIDATION_RULES_STORAGE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_VALIDATION_RULES_STORAGE_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"

namespace i18n {
namespace addressinput {
class Storage;
}
}

class JsonPrefStore;

namespace autofill {

// Creates Storage objects, all of which are backed by a common pref store.
// Adapted for iOS from
// chrome/browser/autofill/validation_rules_storage_factory.{cc,h}, to use
// storage paths specific to iOS.
class ValidationRulesStorageFactory {
 public:
  static std::unique_ptr<::i18n::addressinput::Storage> CreateStorage();

  ValidationRulesStorageFactory(const ValidationRulesStorageFactory&) = delete;
  ValidationRulesStorageFactory& operator=(
      const ValidationRulesStorageFactory&) = delete;

 private:
  friend class base::NoDestructor<ValidationRulesStorageFactory>;

  ValidationRulesStorageFactory();
  ~ValidationRulesStorageFactory();

  scoped_refptr<JsonPrefStore> json_pref_store_;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_VALIDATION_RULES_STORAGE_FACTORY_H_
