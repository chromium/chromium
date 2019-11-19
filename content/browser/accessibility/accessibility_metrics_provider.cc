// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_metrics_provider.h"

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

AccessibilityMetricsProvider::AccessibilityMetricsProvider() {}

AccessibilityMetricsProvider::~AccessibilityMetricsProvider() {}

void AccessibilityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  content::BrowserAccessibilityStateImpl::GetInstance()
      ->UpdateUniqueUserHistograms();
}
