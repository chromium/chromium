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

StoredPaymentApp::~StoredPaymentApp() = default;

}  // namespace content