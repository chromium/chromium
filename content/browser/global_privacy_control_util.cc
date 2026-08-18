// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/global_privacy_control_util.h"

#include <atomic>
#include <optional>

namespace content {

static std::atomic<std::optional<bool>>
    g_global_privacy_control_devtools_override;

void UpdateGlobalPrivacyControlDevToolsOverride(bool new_gpc) {
  g_global_privacy_control_devtools_override.store(new_gpc);
}

void ResetGlobalPrivacyControlDevToolsOverride() {
  g_global_privacy_control_devtools_override.store(std::nullopt);
}

bool IsGlobalPrivacyControlSettingEnabled() {
  std::optional<bool> global_privacy_control_devtools_override =
      g_global_privacy_control_devtools_override.load();
  if (global_privacy_control_devtools_override) {
    return *global_privacy_control_devtools_override;
  }
  // TODO(crbug.com/40745270): This is where a profile setting would be returned
  // if it existed.
  return false;
}

}  // namespace content
