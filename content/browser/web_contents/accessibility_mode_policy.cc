// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/accessibility_mode_policy.h"

#include "base/feature_list.h"
#include "content/browser/web_contents/immediate_accessibility_mode_policy.h"
#include "content/browser/web_contents/progressive_accessibility_mode_policy.h"

namespace content {

namespace {

// Causes the browser to progressively enable accessibility for tabs as they
// are unhidden and disable accessibility as they become hidden.
BASE_FEATURE(kProgressiveAccessibility,
             "ProgressiveAccessibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

enum class ProgressiveMode {
  // Application of mode flags is deferred for hidden WebContents, but otherwise
  // never cleared.
  kOnlyEnable,

  // Application of mode flags is deferred for hidden WebContents, and mode
  // flags are cleared when a WebContents is hidden.
  kDisableOnHide,
};

constexpr base::FeatureParam<ProgressiveMode>::Option
    kProgressiveModeOptions[] = {
        {ProgressiveMode::kOnlyEnable, "only_enable"},
        {ProgressiveMode::kDisableOnHide, "disable_on_hide"}};

BASE_FEATURE_ENUM_PARAM(ProgressiveMode,
                        kProgressiveModeParam,
                        &kProgressiveAccessibility,
                        "mode",
                        ProgressiveMode::kOnlyEnable,
                        &kProgressiveModeOptions);

}  // namespace

// static
std::unique_ptr<AccessibilityModePolicy> AccessibilityModePolicy::Create(
    WebContentsImpl& web_contents) {
  if (base::FeatureList::IsEnabled(kProgressiveAccessibility)) {
    return std::make_unique<ProgressiveAccessibilityModePolicy>(
        web_contents,
        kProgressiveModeParam.Get() == ProgressiveMode::kDisableOnHide);
  }
  return std::make_unique<ImmediateAccessibilityModePolicy>(web_contents);
}

}  // namespace content
