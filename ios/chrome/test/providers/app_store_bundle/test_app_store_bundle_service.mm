// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/app_store_bundle/test_app_store_bundle_service.h"

int TestAppStoreBundleService::GetInstalledAppCount() {
  return app_count_;
}

void TestAppStoreBundleService::PresentAppStoreBundlePromo(
    UIViewController* base_view_controller,
    ProceduralBlock dismiss_handler) {
  dismiss_handler_ = dismiss_handler;
}

void TestAppStoreBundleService::set_installed_app_count(int count) {
  app_count_ = count;
}

void TestAppStoreBundleService::dismiss_promo() {
  if (!dismiss_handler_) {
    return;
  }
  dismiss_handler_();
  dismiss_handler_ = nullptr;
}
