// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace page_info {
class AboutThisSiteService;
}

// This factory helps construct and find the AboutThisSiteService instance for a
// Profile.
class AboutThisSiteServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static page_info::AboutThisSiteService* GetForProfile(ProfileIOS* profile);
  static AboutThisSiteServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AboutThisSiteServiceFactory>;

  AboutThisSiteServiceFactory();
  ~AboutThisSiteServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
