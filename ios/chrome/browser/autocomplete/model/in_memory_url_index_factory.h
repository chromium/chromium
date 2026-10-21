// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_

#include <memory>

#include "components/omnibox/browser/in_memory_url_index.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

// Singleton that owns all InMemoryURLIndex instances and associates them
// with profiles.
class InMemoryURLIndexFactory final
    : public TypedProfileKeyedServiceFactoryIOS<InMemoryURLIndexFactory,
                                                InMemoryURLIndex> {
 public:
  explicit InMemoryURLIndexFactory(PassKey key);

  // Returns the default factory used to build InMemoryURLIndexes. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_IN_MEMORY_URL_INDEX_FACTORY_H_
