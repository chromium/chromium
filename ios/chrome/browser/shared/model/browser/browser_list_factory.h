// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_

#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR profiles.
class BrowserListFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BrowserListFactory,
                                                BrowserList> {
 public:
  explicit BrowserListFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
