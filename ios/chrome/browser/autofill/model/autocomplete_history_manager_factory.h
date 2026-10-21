// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_

#import <memory>

#import "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace autofill {

// Singleton that owns all AutocompleteHistoryManagers and associates them with
// profiles.
class AutocompleteHistoryManagerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AutocompleteHistoryManagerFactory,
          AutocompleteHistoryManager> {
 public:
  explicit AutocompleteHistoryManagerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
