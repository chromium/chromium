// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature_factory.h"

#import <utility>

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/web/model/java_script_console/java_script_console_feature.h"

JavaScriptConsoleFeatureFactory::JavaScriptConsoleFeatureFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "JavaScriptConsoleFeature",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
JavaScriptConsoleFeatureFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<JavaScriptConsoleFeature>();
}
