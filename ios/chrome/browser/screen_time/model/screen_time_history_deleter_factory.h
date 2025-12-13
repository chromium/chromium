// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class ScreenTimeHistoryDeleter;

// Factory that owns and associates a ScreenTimeHistoryDeleter with
// ProfileIOS.
class ScreenTimeHistoryDeleterFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ScreenTimeHistoryDeleter* GetForProfile(ProfileIOS* profile);
  static ScreenTimeHistoryDeleterFactory* GetInstance();

 private:
  friend class base::NoDestructor<ScreenTimeHistoryDeleterFactory>;

  ScreenTimeHistoryDeleterFactory();
  ~ScreenTimeHistoryDeleterFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_MODEL_SCREEN_TIME_HISTORY_DELETER_FACTORY_H_
