// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BrowserList;
class ProfileIOS;

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR profiles.
class BrowserListFactory final : public ProfileKeyedServiceFactoryIOS {
 public:
  static BrowserList* GetForProfile(ProfileIOS* profile);

  // Getter for singleton instance.
  static BrowserListFactory* GetInstance();

 private:
  friend class base::NoDestructor<BrowserListFactory>;

  BrowserListFactory();

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
