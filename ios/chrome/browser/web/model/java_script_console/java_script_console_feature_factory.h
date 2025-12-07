// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class JavaScriptConsoleFeature;

// Singleton that owns all JavaScriptConsoleFeatures and associates them with
// a profile.
class JavaScriptConsoleFeatureFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static JavaScriptConsoleFeatureFactory* GetInstance();
  static JavaScriptConsoleFeature* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<JavaScriptConsoleFeatureFactory>;

  JavaScriptConsoleFeatureFactory();
  ~JavaScriptConsoleFeatureFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
