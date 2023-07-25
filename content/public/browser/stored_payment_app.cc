// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/stored_payment_app.h"

namespace content {

StoredRelatedApplication::StoredRelatedApplication() = default;

StoredRelatedApplication::~StoredRelatedApplication() = default;

StoredCapabilities::StoredCapabilities() = default;

StoredCapabilities::StoredCapabilities(const StoredCapabilities&) = default;

StoredCapabilities::~StoredCapabilities() = default;

StoredPaymentApp::StoredPaymentApp() = default;

StoredPaymentApp::StoredPaymentApp(const StoredPaymentApp& other)
    : registration_id(other.registration_id),
      scope(other.scope),
      name(other.name),
      icon(other.icon ? std::make_unique<SkBitmap>(*other.icon) : nullptr),
      enabled_methods(other.enabled_methods),
      has_explicitly_verified_methods(other.has_explicitly_verified_methods),
      capabilities(other.capabilities),
      prefer_related_applications(other.prefer_related_applications),
      related_applications(other.related_applications),
      user_hint(other.user_hint),
      supported_delegations(other.supported_delegations) {}

StoredPaymentApp::~StoredPaymentApp() = default;

}  // namespace content
