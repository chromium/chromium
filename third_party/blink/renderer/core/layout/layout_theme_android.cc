// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_android.h"

#include "ui/base/ui_base_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeAndroid::Create() {
  return base::AdoptRef(new LayoutThemeAndroid());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeAndroid::Create()));
  return *layout_theme;
}

LayoutThemeAndroid::~LayoutThemeAndroid() {}

String LayoutThemeAndroid::ExtraDefaultStyleSheet() {
  String extra_sheet = LayoutThemeMobile::ExtraDefaultStyleSheet();
  if (features::IsFormControlsRefreshEnabled())
    return extra_sheet;

  // "32px" comes from
  // 2 * -LayoutThemeDefault::SliderTickOffsetFromTrackCenter().
  return extra_sheet + R"CSS(
input[type="range" i]:-internal-has-datalist::-webkit-slider-container {
    min-block-size: 32px;
})CSS";
}

}  // namespace blink
