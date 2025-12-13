// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_STORE_BUNDLE_APP_STORE_BUNDLE_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_STORE_BUNDLE_APP_STORE_BUNDLE_API_H_

#import <memory>

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"

namespace ios {
namespace provider {
// Creates the service for app store bundle.
std::unique_ptr<AppStoreBundleService> CreateAppStoreBundleService();
}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_STORE_BUNDLE_APP_STORE_BUNDLE_API_H_
