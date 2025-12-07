// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/app_store_bundle/test_app_store_bundle_service.h"
#import "ios/public/provider/chrome/browser/app_store_bundle/app_store_bundle_api.h"

namespace ios {
namespace provider {
std::unique_ptr<AppStoreBundleService> CreateAppStoreBundleService() {
  return std::make_unique<TestAppStoreBundleService>();
}
}  // namespace provider
}  // namespace ios
