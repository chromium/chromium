// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_HELPER_H_

#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class WebThemeEngineHelper {
 public:
  PLATFORM_EXPORT static WebThemeEngine* GetNativeThemeEngine();
  PLATFORM_EXPORT static void DidUpdateRendererPreferences(
      const blink::RendererPreferences& renderer_prefs);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_HELPER_H_
