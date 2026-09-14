// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_ANDROID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_ANDROID_H_

#include "third_party/blink/renderer/platform/theme/web_theme_engine_default.h"

namespace blink {

class WebThemeEngineAndroid : public WebThemeEngineDefault {
 public:
  ~WebThemeEngineAndroid() override;
  gfx::Size GetSize(blink::WebThemeEngine::Part) override;
  void GetOverlayScrollbarStyle(
      blink::WebThemeEngine::ScrollbarStyle*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_ANDROID_H_
