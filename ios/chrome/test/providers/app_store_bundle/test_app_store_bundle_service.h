// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_APP_STORE_BUNDLE_TEST_APP_STORE_BUNDLE_SERVICE_H_
#define IOS_CHROME_TEST_PROVIDERS_APP_STORE_BUNDLE_TEST_APP_STORE_BUNDLE_SERVICE_H_

#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"

// Fake app store bundle service for unit testing purpose.
class TestAppStoreBundleService : public AppStoreBundleService {
 public:
  TestAppStoreBundleService() = default;
  TestAppStoreBundleService(const TestAppStoreBundleService&) = delete;
  TestAppStoreBundleService& operator=(const TestAppStoreBundleService&) =
      delete;

  // AppStoreBundleService
  int GetInstalledAppCount() final;
  void PresentAppStoreBundlePromo(UIViewController* base_view_controller,
                                  ProceduralBlock dismiss_handler) final;

  // Methods for testing.
  void set_installed_app_count(int count);
  void dismiss_promo();

 private:
  // Number of install app count used for testing.
  int app_count_;
  // The dismiss handler for app bundle promo.
  ProceduralBlock dismiss_handler_;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_APP_STORE_BUNDLE_TEST_APP_STORE_BUNDLE_SERVICE_H_
