// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/runtime_enabled_feature_checks.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool g_test_feature_allowed = true;

}  // namespace

// static
bool RuntimeEnabledFeaturesBase::CanEnableTestFeature() {
  CHECK(g_test_feature_allowed);
  return true;
}

ScopedTestFeatureAllowedForTest::ScopedTestFeatureAllowedForTest(bool allowed)
    : original_(g_test_feature_allowed) {
  g_test_feature_allowed = allowed;
}

ScopedTestFeatureAllowedForTest::~ScopedTestFeatureAllowedForTest() {
  g_test_feature_allowed = original_;
}

}  // namespace blink
