// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_

#import <memory>

#import "components/keyed_service/core/keyed_service.h"

// BackendPromoService is responsible for backend promo features.
class BackendPromoService : public KeyedService {
 public:
  BackendPromoService() = default;

  BackendPromoService(const BackendPromoService&) = delete;
  BackendPromoService& operator=(const BackendPromoService&) = delete;

  ~BackendPromoService() override = default;
};

namespace ios::provider {

// Creates a new instance of BackendPromoService.
std::unique_ptr<BackendPromoService> CreateBackendPromoService();

}  // namespace ios::provider

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_MODEL_BACKEND_PROMO_SERVICE_H_
