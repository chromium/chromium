// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"

#include "base/containers/fixed_flat_map.h"
#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-shared.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

using WTE = WebThemeEngine;
using NT = ui::NativeTheme;

NT::Part NativeThemePart(WTE::Part part) {
  static constexpr auto kPartMap = base::MakeFixedFlatMap<WTE::Part, NT::Part>(
      {{WTE::kPartScrollbarDownArrow, NT::kScrollbarDownArrow},
       {WTE::kPartScrollbarLeftArrow, NT::kScrollbarLeftArrow},
       {WTE::kPartScrollbarRightArrow, NT::kScrollbarRightArrow},
       {WTE::kPartScrollbarUpArrow, NT::kScrollbarUpArrow},
       {WTE::kPartScrollbarHorizontalThumb, NT::kScrollbarHorizontalThumb},
       {WTE::kPartScrollbarVerticalThumb, NT::kScrollbarVerticalThumb},
       {WTE::kPartScrollbarHorizontalTrack, NT::kScrollbarHorizontalTrack},
       {WTE::kPartScrollbarVerticalTrack, NT::kScrollbarVerticalTrack},
       {WTE::kPartScrollbarCorner, NT::kScrollbarCorner},
       {WTE::kPartCheckbox, NT::kCheckbox},
       {WTE::kPartRadio, NT::kRadio},
       {WTE::kPartButton, NT::kPushButton},
       {WTE::kPartTextField, NT::kTextField},
       {WTE::kPartMenuList, NT::kMenuList},
       {WTE::kPartSliderTrack, NT::kSliderTrack},
       {WTE::kPartSliderThumb, NT::kSliderThumb},
       {WTE::kPartInnerSpinButton, NT::kInnerSpinButton},
       {WTE::kPartProgressBar, NT::kProgressBar}});
  return kPartMap.at(part);
}

NT::State NativeThemeState(WTE::State state) {
  static constexpr auto kStateMap =
      base::MakeFixedFlatMap<WTE::State, NT::State>(
          {{WTE::kStateDisabled, NT::kDisabled},
           {WTE::kStateHover, NT::kHovered},
           {WTE::kStateNormal, NT::kNormal},
           {WTE::kStatePressed, NT::kPressed}});
  return kStateMap.at(state);
}

NT::PreferredColorScheme NativeColorScheme(
    mojom::blink::ColorScheme color_scheme) {
  using MCS = mojom::blink::ColorScheme;
  using NTPCS = NT::PreferredColorScheme;
  static constexpr auto kColorSchemeMap = base::MakeFixedFlatMap<MCS, NTPCS>(
      {{MCS::kLight, NTPCS::kLight}, {MCS::kDark, NTPCS::kDark}});
  return kColorSchemeMap.at(color_scheme);
}

NT::PreferredContrast NativeContrast(mojom::blink::PreferredContrast contrast) {
  using MPC = mojom::blink::PreferredContrast;
  using NTPC = NT::PreferredContrast;
  static constexpr auto kContrastMap = base::MakeFixedFlatMap<MPC, NTPC>(
      {{MPC::kMore, NTPC::kMore},
       {MPC::kLess, NTPC::kLess},
       {MPC::kNoPreference, NTPC::kNoPreference},
       {MPC::kCustom, NTPC::kCustom}});
  return kContrastMap.at(contrast);
}

}  // namespace blink
