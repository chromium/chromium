// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"

class ChromiumBackendPromoService final : public BackendPromoService {
 public:
  ChromiumBackendPromoService() = default;
  ~ChromiumBackendPromoService() final = default;
};

namespace ios::provider {

std::unique_ptr<BackendPromoService> CreateBackendPromoService() {
  return std::make_unique<ChromiumBackendPromoService>();
}

}  // namespace ios::provider
