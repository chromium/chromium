// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_MODEL_IOS_AT_MEMORY_QUERY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_MODEL_IOS_AT_MEMORY_QUERY_SERVICE_FACTORY_H_

#import "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// KeyedService factory for `AtMemoryQueryService`.
class IOSAtMemoryQueryServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSAtMemoryQueryServiceFactory,
          autofill::AtMemoryQueryService> {
 public:
  explicit IOSAtMemoryQueryServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_MODEL_IOS_AT_MEMORY_QUERY_SERVICE_FACTORY_H_
