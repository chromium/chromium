// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace blink {

TEST(ManifestUtilTest, DisplayModeConversions) {
  struct ReversibleConversion {
    blink::mojom::DisplayMode display_mode;
    std::string lowercase_display_mode_string;
  } reversible_conversions[] = {
      {blink::mojom::DisplayMode::kUndefined, ""},
      {blink::mojom::DisplayMode::kBrowser, "browser"},
      {blink::mojom::DisplayMode::kMinimalUi, "minimal-ui"},
      {blink::mojom::DisplayMode::kStandalone, "standalone"},
      {blink::mojom::DisplayMode::kFullscreen, "fullscreen"},
  };

  for (const ReversibleConversion& conversion : reversible_conversions) {
    EXPECT_EQ(conversion.display_mode,
              DisplayModeFromString(conversion.lowercase_display_mode_string));
    EXPECT_EQ(conversion.lowercase_display_mode_string,
              DisplayModeToString(conversion.display_mode));
  }

  // DisplayModeFromString() should work with non-lowercase strings.
  EXPECT_EQ(blink::mojom::DisplayMode::kFullscreen,
            DisplayModeFromString("Fullscreen"));

  // DisplayModeFromString() should return
  // DisplayMode::kUndefined if the string isn't known.
  EXPECT_EQ(blink::mojom::DisplayMode::kUndefined,
            DisplayModeFromString("random"));
}

TEST(ManifestUtilTest, WebScreenOrientationLockTypeConversions) {
  struct ReversibleConversion {
    blink::WebScreenOrientationLockType orientation;
    std::string lowercase_orientation_string;
  } reversible_conversions[] = {
      {blink::kWebScreenOrientationLockDefault, ""},
      {blink::kWebScreenOrientationLockPortraitPrimary, "portrait-primary"},
      {blink::kWebScreenOrientationLockPortraitSecondary, "portrait-secondary"},
      {blink::kWebScreenOrientationLockLandscapePrimary, "landscape-primary"},
      {blink::kWebScreenOrientationLockLandscapeSecondary,
       "landscape-secondary"},
      {blink::kWebScreenOrientationLockAny, "any"},
      {blink::kWebScreenOrientationLockLandscape, "landscape"},
      {blink::kWebScreenOrientationLockPortrait, "portrait"},
      {blink::kWebScreenOrientationLockNatural, "natural"},
  };

  for (const ReversibleConversion& conversion : reversible_conversions) {
    EXPECT_EQ(conversion.orientation,
              WebScreenOrientationLockTypeFromString(
                  conversion.lowercase_orientation_string));
    EXPECT_EQ(conversion.lowercase_orientation_string,
              WebScreenOrientationLockTypeToString(conversion.orientation));
  }

  // WebScreenOrientationLockTypeFromString() should work with non-lowercase
  // strings.
  EXPECT_EQ(blink::kWebScreenOrientationLockNatural,
            WebScreenOrientationLockTypeFromString("Natural"));

  // WebScreenOrientationLockTypeFromString() should return
  // blink::WebScreenOrientationLockDefault if the string isn't known.
  EXPECT_EQ(blink::kWebScreenOrientationLockDefault,
            WebScreenOrientationLockTypeFromString("random"));
}

}  // namespace blink
