// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
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
      {blink::mojom::DisplayMode::kUnframed, "unframed"},
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

TEST(ManifestUtilTest, IsDefaultManifest) {
  GURL document_url("https://example.com/app/index.html");
  auto default_manifest = Manifest::CreateDefault(document_url);
  ASSERT_TRUE(default_manifest.has_value());

  // An unmodified default manifest is recognized as default.
  EXPECT_TRUE(IsDefaultManifest(*default_manifest, document_url));

  // Mismatched document URL returns false.
  EXPECT_FALSE(IsDefaultManifest(*default_manifest,
                                 GURL("https://example.com/other/index.html")));

  // Specifying has_custom_id makes it non-default.
  auto custom_id_builder = default_manifest->ToBuilder();
  custom_id_builder.SetHasCustomId(true);
  Manifest custom_id_manifest = std::move(custom_id_builder).BuildChecked();
  EXPECT_FALSE(IsDefaultManifest(custom_id_manifest, document_url));

  // Specifying has_valid_specified_start_url makes it non-default.
  auto specified_start_url_builder = default_manifest->ToBuilder();
  specified_start_url_builder.SetHasValidSpecifiedStartUrl(true);
  Manifest specified_start_url_manifest =
      std::move(specified_start_url_builder).BuildChecked();
  EXPECT_FALSE(IsDefaultManifest(specified_start_url_manifest, document_url));

  // Setting a name makes it non-default.
  auto named_builder = default_manifest->ToBuilder();
  named_builder.SetName(u"App Name");
  Manifest named_manifest = std::move(named_builder).BuildChecked();
  EXPECT_FALSE(IsDefaultManifest(named_manifest, document_url));

  // Calling with an invalid or opaque document URL returns false.
  EXPECT_FALSE(IsDefaultManifest(*default_manifest, GURL()));
  EXPECT_FALSE(
      IsDefaultManifest(*default_manifest, GURL("data:text/html,Hello")));
}

}  // namespace blink
