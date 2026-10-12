// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_preferences_util.h"

#include <optional>

#include "content/browser/global_privacy_control_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

void UpdateRendererPreferencesForWorkerHelper(
    BrowserContext* context,
    blink::RendererPreferences* preferences) {
  CHECK(preferences);
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      context, preferences);
  // Set the DevTools GPC override, if one is set. This takes precedence
  // over the embedder's profile setting (see
  // `IsGlobalPrivacyControlFeatureAndSettingEnabled()`).
  preferences->global_privacy_control_devtools_override =
      GetGlobalPrivacyControlDevToolsOverride();
}

}  // namespace content
