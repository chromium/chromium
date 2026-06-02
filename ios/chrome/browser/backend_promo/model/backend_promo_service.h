// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_

#import <memory>

#import "components/keyed_service/core/keyed_service.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// BackendPromoService is responsible for backend promo features.
class BackendPromoService : public KeyedService {
 public:
  BackendPromoService() = default;

  BackendPromoService(const BackendPromoService&) = delete;
  BackendPromoService& operator=(const BackendPromoService&) = delete;

  ~BackendPromoService() override = default;
};

class BrowserList;
namespace signin {
class IdentityManager;
}  // namespace signin

namespace ios::provider {

// Creates a new instance of BackendPromoService.
std::unique_ptr<BackendPromoService> CreateBackendPromoService(
    signin::IdentityManager* identity_manager,
    BrowserList* browser_list,
    feature_engagement::Tracker* tracker);

// Shows the backend promo debug tools.
void ShowBackendPromoDebugTools();

}  // namespace ios::provider

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_
