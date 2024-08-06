// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/theme_helper.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/renderer.mojom.h"

namespace content {

// static
ThemeHelper* ThemeHelper::GetInstance() {
  static base::NoDestructor<ThemeHelper> s_theme_helper;
  return s_theme_helper.get();
}

ThemeHelper::ThemeHelper() : theme_observation_(this) {
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForWeb());
}

ThemeHelper::~ThemeHelper() {}

mojom::UpdateSystemColorInfoParamsPtr MakeUpdateSystemColorInfoParams(
    ui::NativeTheme* native_theme) {
  mojom::UpdateSystemColorInfoParamsPtr params =
      mojom::UpdateSystemColorInfoParams::New();
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  params->accent_color = native_theme->user_color();
#endif

  return params;
}

void ThemeHelper::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK(theme_observation_.IsObservingSource(observed_theme));

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
