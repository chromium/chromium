// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"

// Singleton that owns all JavaScriptConsoleFeatures and associates them with
// a profile.
class JavaScriptConsoleFeatureFactory final
    : public TypedProfileKeyedServiceFactoryIOS<JavaScriptConsoleFeatureFactory,
                                                JavaScriptConsoleFeature> {
 public:
  explicit JavaScriptConsoleFeatureFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
