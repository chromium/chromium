// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/runtime_enabled_feature_checks.h"

#include <mutex>

#include "base/check.h"
#include "base/memory/protected_memory.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

DEFINE_PROTECTED_DATA base::ProtectedMemory<bool> g_mojo_js_per_context_allowed;
DEFINE_PROTECTED_DATA base::ProtectedMemory<bool>
    g_mojo_js_runtime_feature_allowed;
DEFINE_PROTECTED_DATA base::ProtectedMemory<bool>
    g_mojo_js_test_runtime_feature_allowed;

void SetAllowed(base::ProtectedMemory<bool>& allowed, bool value) {
  if (*allowed == value) {
    // No need to make the storage writable again.
    return;
  }
  base::AutoWritableMemory writer(allowed);
  writer.GetProtectedData() = value;
}

bool g_test_feature_allowed = true;

}  // namespace

void InitializeMojoJSPermissions() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    base::ProtectedMemoryInitializer per_context_initializer(
        g_mojo_js_per_context_allowed, false);
    base::ProtectedMemoryInitializer runtime_feature_initializer(
        g_mojo_js_runtime_feature_allowed, false);
    base::ProtectedMemoryInitializer test_runtime_feature_initializer(
        g_mojo_js_test_runtime_feature_allowed, false);
  });
}

void AllowMojoJSPerContextForProcess() {
  SetAllowed(g_mojo_js_per_context_allowed, true);
}

void AllowMojoJSForProcess() {
  SetAllowed(g_mojo_js_runtime_feature_allowed, true);
}

void AllowMojoJSTestForProcess() {
  SetAllowed(g_mojo_js_test_runtime_feature_allowed, true);
}

bool IsMojoJSAllowedPerContextForProcess() {
  return *g_mojo_js_per_context_allowed;
}

bool IsMojoJSAllowedForProcess() {
  return *g_mojo_js_runtime_feature_allowed;
}

bool IsMojoJSTestAllowedForProcess() {
  return *g_mojo_js_test_runtime_feature_allowed;
}

void SetMojoJSPermissionsForTesting(bool per_context,
                                    bool runtime_feature,
                                    bool test_runtime_feature) {
  SetAllowed(g_mojo_js_per_context_allowed, per_context);
  SetAllowed(g_mojo_js_runtime_feature_allowed, runtime_feature);
  SetAllowed(g_mojo_js_test_runtime_feature_allowed, test_runtime_feature);
}

// static
bool RuntimeEnabledFeaturesBase::CanEnableMojoJS() {
  CHECK(IsMojoJSAllowedForProcess());
  return true;
}

// static
bool RuntimeEnabledFeaturesBase::CanEnableMojoJSTest() {
  CHECK(IsMojoJSTestAllowedForProcess());
  return true;
}

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
