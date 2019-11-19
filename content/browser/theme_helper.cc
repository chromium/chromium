// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/theme_helper.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/renderer.mojom.h"

namespace content {

// static
ThemeHelper* ThemeHelper::GetInstance() {
  static base::NoDestructor<ThemeHelper> s_theme_helper;
  return s_theme_helper.get();
}

ThemeHelper::ThemeHelper() : theme_observer_(this) {
  theme_observer_.Add(ui::NativeTheme::GetInstanceForWeb());
}

ThemeHelper::~ThemeHelper() {}

mojom::UpdateSystemColorInfoParamsPtr MakeUpdateSystemColorInfoParams(
    ui::NativeTheme* native_theme) {
  mojom::UpdateSystemColorInfoParamsPtr params =
      mojom::UpdateSystemColorInfoParams::New();
  params->is_dark_mode = native_theme->ShouldUseDarkColors();
  params->is_high_contrast = native_theme->UsesHighContrastColors();
  params->preferred_color_scheme = native_theme->GetPreferredColorScheme();
  const auto& colors = native_theme->GetSystemColors();
  params->colors.insert(colors.begin(), colors.end());

  return params;
}

void ThemeHelper::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK(theme_observer_.IsObserving(observed_theme));

  mojom::UpdateSystemColorInfoParamsPtr params =
      MakeUpdateSystemColorInfoParams(observed_theme);
  for (auto iter = RenderProcessHost::AllHostsIterator(); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.GetCurrentValue()->IsInitializedAndNotDead()) {
      iter.GetCurrentValue()->GetRendererInterface()->UpdateSystemColorInfo(
          params->Clone());
    }
  }
}

void ThemeHelper::SendSystemColorInfo(mojom::Renderer* renderer) const {
  mojom::UpdateSystemColorInfoParamsPtr params =
      MakeUpdateSystemColorInfoParams(ui::NativeTheme::GetInstanceForWeb());
  renderer->UpdateSystemColorInfo(std::move(params));
}

}  // namespace content
