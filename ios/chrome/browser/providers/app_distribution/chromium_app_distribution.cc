// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"

namespace ios {
namespace provider {

std::string GetBrandCode() {
  // Chromium has no brand code.
  return std::string();
}

void ScheduleAppDistributionNotifications(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    bool is_first_run) {
  // Nothing to do for Chromium.
}

void CancelAppDistributionNotifications() {
  // Nothing to do for Chromium.
}

void InitializeFirebase(base::Time install_date, bool is_first_run) {
  // Nothing to do for Chromium.
}

}  // namespace provider
}  // namespace ios
