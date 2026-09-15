// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_RUNTIME_ENABLED_FEATURE_CHECKS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_RUNTIME_ENABLED_FEATURE_CHECKS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// The checks named by `custom_enable_check` in runtime_enabled_features.json5
// are defined in runtime_enabled_feature_checks.cc. This header declares the
// helpers (test or otherwise) to configure state that backs those checks.

// MojoJS and related features are gated by protected memory, which needs to be
// explicitly initialized before use.
PLATFORM_EXPORT void InitializeMojoJSPermissions();

// Grants this process permission to mark individual execution contexts as
// MojoJS-enabled via ContextFeatureSettings; this is the typical grant for
// WebUI pages and privileged extensions that use MojoJS.
//
// TODO(dcheng): It's not clear that there's a lot of value in having this be
// a separate toggle from `AllowMojoJSForProcess()`.
PLATFORM_EXPORT void AllowMojoJSPerContextForProcess();

PLATFORM_EXPORT void AllowMojoJSForProcess();
PLATFORM_EXPORT void AllowMojoJSTestForProcess();

PLATFORM_EXPORT bool IsMojoJSAllowedPerContextForProcess();
PLATFORM_EXPORT bool IsMojoJSAllowedForProcess();
PLATFORM_EXPORT bool IsMojoJSTestAllowedForProcess();

// Permission grants are irrevocable except for testing purposes.
PLATFORM_EXPORT void SetMojoJSPermissionsForTesting(bool per_context,
                                                    bool runtime_feature,
                                                    bool test_runtime_feature);

// RAII scoper to control the TestFeatureCustomEnableCheck runtime feature.
class PLATFORM_EXPORT ScopedTestFeatureAllowedForTest {
  STACK_ALLOCATED();

 public:
  explicit ScopedTestFeatureAllowedForTest(bool allowed);
  ScopedTestFeatureAllowedForTest(const ScopedTestFeatureAllowedForTest&) =
      delete;
  ScopedTestFeatureAllowedForTest& operator=(
      const ScopedTestFeatureAllowedForTest&) = delete;
  ~ScopedTestFeatureAllowedForTest();

 private:
  const bool original_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_RUNTIME_ENABLED_FEATURE_CHECKS_H_
