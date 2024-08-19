// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace page_info {
class AboutThisSiteService;
}

// This factory helps construct and find the AboutThisSiteService instance for a
// Browser State.
class AboutThisSiteServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static page_info::AboutThisSiteService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static AboutThisSiteServiceFactory* GetInstance();

  AboutThisSiteServiceFactory(const AboutThisSiteServiceFactory&) = delete;
  AboutThisSiteServiceFactory& operator=(const AboutThisSiteServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AboutThisSiteServiceFactory>;

  AboutThisSiteServiceFactory();
  ~AboutThisSiteServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_ABOUT_THIS_SITE_SERVICE_FACTORY_H_
