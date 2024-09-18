// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_

#import <CoreFoundation/CoreFoundation.h>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ScreenTimeHistoryDeleter;

// Factory that owns and associates a ScreenTimeHistoryDeleter with
// ProfileIOS.
class ScreenTimeHistoryDeleterFactory : public BrowserStateKeyedServiceFactory {
 public:
  static ScreenTimeHistoryDeleter* GetForProfile(ProfileIOS* profile);

  static ScreenTimeHistoryDeleterFactory* GetInstance();

  ScreenTimeHistoryDeleterFactory(const ScreenTimeHistoryDeleterFactory&) =
      delete;
  ScreenTimeHistoryDeleterFactory& operator=(
      const ScreenTimeHistoryDeleterFactory&) = delete;

 private:
  friend class base::NoDestructor<ScreenTimeHistoryDeleterFactory>;

  ScreenTimeHistoryDeleterFactory();
  ~ScreenTimeHistoryDeleterFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
