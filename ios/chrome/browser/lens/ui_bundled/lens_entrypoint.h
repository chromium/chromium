// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_UI_BUNDLED_LENS_ENTRYPOINT_H_
#define IOS_CHROME_BROWSER_LENS_UI_BUNDLED_LENS_ENTRYPOINT_H_

// Enum representing the possible Lens entrypoints on iOS.
enum class LensEntrypoint {
  ContextMenu = 0,
  HomeScreenWidget = 1,
  NewTabPage = 2,
  Keyboard = 3,
  Spotlight = 4,
  OmniboxPostCapture = 5,
  ImageShareMenu = 6,
  AppIconLongPress = 7,
  PlusButton = 8,
  WebSearchBar = 9,
  TranslateOnebox = 10,
  Intents = 11,
  WebImagesSearchBar = 12,
  WhatsNewPromo = 13,
  LensOverlayLocationBar = 14,
  LensOverlayOverflowMenu = 15,
  LensOverlayLvfEscapeHatch = 16,
  LensOverlayLvfShutterButton = 17,
  LensOverlayLvfGallery = 18,
  LensOverlayAIHub = 19,
  LensOverlayFREPromo = 20,
  kMaxValue = LensOverlayFREPromo,
};

extern const char kIOSLensEntrypoint[];

#endif  // IOS_CHROME_BROWSER_LENS_UI_BUNDLED_LENS_ENTRYPOINT_H_
