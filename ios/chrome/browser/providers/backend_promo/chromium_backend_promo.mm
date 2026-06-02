// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"

class ChromiumBackendPromoService final : public BackendPromoService {
 public:
  ChromiumBackendPromoService() = default;
  ~ChromiumBackendPromoService() final = default;
};

class BrowserList;
namespace signin {
class IdentityManager;
}  // namespace signin

namespace ios::provider {

std::unique_ptr<BackendPromoService> CreateBackendPromoService(
    signin::IdentityManager* identity_manager,
    BrowserList* browser_list,
    feature_engagement::Tracker* tracker) {
  return std::make_unique<ChromiumBackendPromoService>();
}

void ShowBackendPromoDebugTools() {
  // Debug tools are not supported in Chromium build.
}

}  // namespace ios::provider
