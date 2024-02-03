// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
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
      {blink::mojom::DisplayMode::kWindowControlsOverlay,
       "window-controls-overlay"},
      {blink::mojom::DisplayMode::kTabbed, "tabbed"},
      {blink::mojom::DisplayMode::kBorderless, "borderless"},
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
    device::mojom::ScreenOrientationLockType orientation;
    std::string lowercase_orientation_string;
  } reversible_conversions[] = {
      {device::mojom::ScreenOrientationLockType::DEFAULT, ""},
      {device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY,
       "portrait-primary"},
      {device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY,
       "portrait-secondary"},
      {device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY,
       "landscape-primary"},
      {device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY,
       "landscape-secondary"},
      {device::mojom::ScreenOrientationLockType::ANY, "any"},
      {device::mojom::ScreenOrientationLockType::LANDSCAPE, "landscape"},
      {device::mojom::ScreenOrientationLockType::PORTRAIT, "portrait"},
      {device::mojom::ScreenOrientationLockType::NATURAL, "natural"},
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
  EXPECT_EQ(device::mojom::ScreenOrientationLockType::NATURAL,
            WebScreenOrientationLockTypeFromString("Natural"));

  // WebScreenOrientationLockTypeFromString() should return
  // blink::WebScreenOrientationLockDefault if the string isn't known.
  EXPECT_EQ(device::mojom::ScreenOrientationLockType::DEFAULT,
            WebScreenOrientationLockTypeFromString("random"));
}

TEST(ManifestUtilTest, CaptureLinksFromString) {
  EXPECT_EQ(blink::mojom::CaptureLinks::kUndefined, CaptureLinksFromString(""));
  EXPECT_EQ(blink::mojom::CaptureLinks::kNone, CaptureLinksFromString("none"));
  EXPECT_EQ(blink::mojom::CaptureLinks::kNewClient,
            CaptureLinksFromString("new-client"));
  EXPECT_EQ(blink::mojom::CaptureLinks::kExistingClientNavigate,
            CaptureLinksFromString("existing-client-navigate"));

  // CaptureLinksFromString() should work with non-lowercase strings.
  EXPECT_EQ(blink::mojom::CaptureLinks::kNewClient,
            CaptureLinksFromString("NEW-CLIENT"));

  // CaptureLinksFromString() should return CaptureLinks::kUndefined if the
  // string isn't known.
  EXPECT_EQ(blink::mojom::CaptureLinks::kUndefined,
            CaptureLinksFromString("unknown-value"));
}

TEST(ManifestUtilTest, LaunchHandlerClientModeFromString) {
  using ClientMode = Manifest::LaunchHandler::ClientMode;
  EXPECT_EQ(std::nullopt, ClientModeFromString(""));
  EXPECT_EQ(ClientMode::kAuto, ClientModeFromString("auto"));
  EXPECT_EQ(ClientMode::kNavigateNew, ClientModeFromString("navigate-new"));
  EXPECT_EQ(ClientMode::kNavigateExisting,
            ClientModeFromString("navigate-existing"));
  EXPECT_EQ(ClientMode::kFocusExisting, ClientModeFromString("focus-existing"));

  // Uppercase spelling.
  EXPECT_EQ(ClientMode::kNavigateNew, ClientModeFromString("NAVIGATE-NEW"));

  // Unknown value.
  EXPECT_EQ(std::nullopt, ClientModeFromString("unknown-value"));
}

}  // namespace blink
