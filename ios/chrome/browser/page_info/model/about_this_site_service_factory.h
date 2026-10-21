// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_MODEL_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_MODEL_ABOUT_THIS_SITE_SERVICE_FACTORY_H_

#import <memory>

#import "components/page_info/core/about_this_site_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// This factory helps construct and find the AboutThisSiteService instance for a
// Profile.
class AboutThisSiteServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AboutThisSiteServiceFactory,
          page_info::AboutThisSiteService> {
 public:
  explicit AboutThisSiteServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_MODEL_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
