// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_APP_STORE_BUNDLE_CHROMIUM_APP_STORE_BUNDLE_SERVICE_H_
#define IOS_CHROME_BROWSER_PROVIDERS_APP_STORE_BUNDLE_CHROMIUM_APP_STORE_BUNDLE_SERVICE_H_

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"

// App store bundle service for Chromium build.
class ChromiumAppStoreBundleService : public AppStoreBundleService {
 public:
  ChromiumAppStoreBundleService() = default;
  ChromiumAppStoreBundleService(const ChromiumAppStoreBundleService&) = delete;
  ChromiumAppStoreBundleService& operator=(
      const ChromiumAppStoreBundleService&) = delete;

  // AppStoreBundleService
  int GetInstalledAppCount() final;
  void PresentAppStoreBundlePromo(UIViewController* base_view_controller,
                                  ProceduralBlock dismiss_handler) final;
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_APP_STORE_BUNDLE_CHROMIUM_APP_STORE_BUNDLE_SERVICE_H_
