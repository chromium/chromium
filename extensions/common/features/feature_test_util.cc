// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature_test_util.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "extensions/common/extensions_client.h"

namespace extensions {

FeatureTestPeer::ScopedDelegatedAvailabilityCheckHandlers::
    ScopedDelegatedAvailabilityCheckHandlers(
        const Feature& feature,
        Feature::DelegatedAvailabilityCheckHandler handler)
    : previous_handlers_(
          ExtensionsClient::Get()->GetFeatureDelegatedAvailabilityCheckMap()) {
  CHECK(handler);
  CHECK(feature.RequiresDelegatedAvailabilityCheck());
  auto handlers = previous_handlers_;
  handlers.insert_or_assign(std::string(feature.name()), handler);
  ExtensionsClient::Get()->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(handlers));
}

FeatureTestPeer::ScopedDelegatedAvailabilityCheckHandlers::
    ScopedDelegatedAvailabilityCheckHandlers(
        Feature::FeatureDelegatedAvailabilityCheckMap handlers)
    : previous_handlers_(
          ExtensionsClient::Get()->GetFeatureDelegatedAvailabilityCheckMap()) {
  ExtensionsClient::Get()->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(handlers));
}

FeatureTestPeer::ScopedDelegatedAvailabilityCheckHandlers::
    ~ScopedDelegatedAvailabilityCheckHandlers() {
  ExtensionsClient::Get()->SetFeatureDelegatedAvailabilityCheckMap(
      std::move(previous_handlers_));
}

}  // namespace extensions
