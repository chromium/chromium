// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BrowserList;

// Keyed service factory for BrowserList.
// This factory returns the same instance for regular and OTR profiles.
class BrowserListFactory final : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static BrowserList* GetForBrowserState(ProfileIOS* profile);

  static BrowserList* GetForProfile(ProfileIOS* profile);

  // Getter for singleton instance.
  static BrowserListFactory* GetInstance();

  // Not copyable or moveable.
  BrowserListFactory(const BrowserListFactory&) = delete;
  BrowserListFactory& operator=(const BrowserListFactory&) = delete;

 private:
  friend class base::NoDestructor<BrowserListFactory>;

  BrowserListFactory();

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const final;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_FACTORY_H_
