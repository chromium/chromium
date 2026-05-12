// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"

// Test BackendPromoService implementation.
class TestBackendPromoService final : public BackendPromoService {
 public:
  TestBackendPromoService() = default;
  ~TestBackendPromoService() override = default;
};

namespace ios::provider {

std::unique_ptr<BackendPromoService> CreateBackendPromoService() {
  return std::make_unique<TestBackendPromoService>();
}

}  // namespace ios::provider
