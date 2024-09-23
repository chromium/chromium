// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class JavaScriptConsoleFeature;

// Singleton that owns all JavaScriptConsoleFeatures and associates them with
// a profile.
class JavaScriptConsoleFeatureFactory : public BrowserStateKeyedServiceFactory {
 public:
  static JavaScriptConsoleFeatureFactory* GetInstance();
  static JavaScriptConsoleFeature* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<JavaScriptConsoleFeatureFactory>;

  JavaScriptConsoleFeatureFactory();
  ~JavaScriptConsoleFeatureFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* browser_state) const override;

  JavaScriptConsoleFeatureFactory(const JavaScriptConsoleFeatureFactory&) =
      delete;
  JavaScriptConsoleFeatureFactory& operator=(
      const JavaScriptConsoleFeatureFactory&) = delete;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_FEATURE_FACTORY_H_
