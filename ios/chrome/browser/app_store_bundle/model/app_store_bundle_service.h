// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_H_
#define IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/keyed_service/core/keyed_service.h"

// A browser-state keyed service that is used to manage the App Store bundle.
class AppStoreBundleService : public KeyedService {
 public:
  AppStoreBundleService();
  ~AppStoreBundleService() override;

  AppStoreBundleService(const AppStoreBundleService&) = delete;
  AppStoreBundleService& operator=(const AppStoreBundleService&) = delete;

  // Number of apps in the bundle already installed.
  virtual int GetInstalledAppCount() = 0;

  // Presents the app store bundle promo.
  virtual void PresentAppStoreBundlePromo(
      UIViewController* base_view_controller,
      ProceduralBlock dismiss_handler) = 0;
};

#endif  // IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_H_
