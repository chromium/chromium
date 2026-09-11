// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace password_manager {
class OnDeviceEncryptionMetricsReporter;
}  // namespace password_manager

// Singleton that creates and owns the OnDeviceEncryptionMetricsReporter for
// each ProfileIOS.
class IOSChromeOnDeviceEncryptionMetricsReporterFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static password_manager::OnDeviceEncryptionMetricsReporter* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeOnDeviceEncryptionMetricsReporterFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      IOSChromeOnDeviceEncryptionMetricsReporterFactory>;

  IOSChromeOnDeviceEncryptionMetricsReporterFactory();
  ~IOSChromeOnDeviceEncryptionMetricsReporterFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
