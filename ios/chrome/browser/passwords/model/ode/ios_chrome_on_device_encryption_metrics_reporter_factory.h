// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_

#import <memory>

#import "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Singleton that creates and owns the OnDeviceEncryptionMetricsReporter for
// each ProfileIOS.
class IOSChromeOnDeviceEncryptionMetricsReporterFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeOnDeviceEncryptionMetricsReporterFactory,
          password_manager::OnDeviceEncryptionMetricsReporter> {
 public:
  explicit IOSChromeOnDeviceEncryptionMetricsReporterFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS:
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ODE_IOS_CHROME_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_FACTORY_H_
