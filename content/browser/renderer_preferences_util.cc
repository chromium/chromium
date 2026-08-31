// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_preferences_util.h"

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
  // TODO(crbug.com/40745270): Rename IsGlobalPrivacyControlSettingEnabled to
  // IsGlobalPrivacyControlDevToolsOverrideEnabled
  preferences->is_global_privacy_control_setting_enabled |=
      IsGlobalPrivacyControlSettingEnabled();
}

}  // namespace content
