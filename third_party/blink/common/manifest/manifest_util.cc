// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include "base/strings/string_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace blink {

std::string DisplayModeToString(blink::mojom::DisplayMode display) {
  switch (display) {
    case blink::mojom::DisplayMode::kUndefined:
      return "";
    case blink::mojom::DisplayMode::kBrowser:
      return "browser";
    case blink::mojom::DisplayMode::kMinimalUi:
      return "minimal-ui";
    case blink::mojom::DisplayMode::kStandalone:
      return "standalone";
    case blink::mojom::DisplayMode::kFullscreen:
      return "fullscreen";
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
      return "window-controls-overlay";
  }
  return "";
}

blink::mojom::DisplayMode DisplayModeFromString(const std::string& display) {
  if (base::LowerCaseEqualsASCII(display, "browser"))
    return blink::mojom::DisplayMode::kBrowser;
  if (base::LowerCaseEqualsASCII(display, "minimal-ui"))
    return blink::mojom::DisplayMode::kMinimalUi;
  if (base::LowerCaseEqualsASCII(display, "standalone"))
    return blink::mojom::DisplayMode::kStandalone;
  if (base::LowerCaseEqualsASCII(display, "fullscreen"))
    return blink::mojom::DisplayMode::kFullscreen;
  if (base::LowerCaseEqualsASCII(display, "window-controls-overlay"))
    return blink::mojom::DisplayMode::kWindowControlsOverlay;
  return blink::mojom::DisplayMode::kUndefined;
}

bool IsBasicDisplayMode(blink::mojom::DisplayMode display) {
  if (display == blink::mojom::DisplayMode::kBrowser ||
      display == blink::mojom::DisplayMode::kMinimalUi ||
      display == blink::mojom::DisplayMode::kStandalone ||
      display == blink::mojom::DisplayMode::kFullscreen) {
    return true;
  }

  return false;
}

std::string WebScreenOrientationLockTypeToString(
    device::mojom::ScreenOrientationLockType orientation) {
  switch (orientation) {
    case device::mojom::ScreenOrientationLockType::DEFAULT:
      return "";
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return "portrait-primary";
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return "portrait-secondary";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return "landscape-primary";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return "landscape-secondary";
    case device::mojom::ScreenOrientationLockType::ANY:
      return "any";
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return "landscape";
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return "portrait";
    case device::mojom::ScreenOrientationLockType::NATURAL:
      return "natural";
  }
  return "";
}

device::mojom::ScreenOrientationLockType WebScreenOrientationLockTypeFromString(
    const std::string& orientation) {
  if (base::LowerCaseEqualsASCII(orientation, "portrait-primary"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
  if (base::LowerCaseEqualsASCII(orientation, "portrait-secondary"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY;
  if (base::LowerCaseEqualsASCII(orientation, "landscape-primary"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
  if (base::LowerCaseEqualsASCII(orientation, "landscape-secondary"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY;
  if (base::LowerCaseEqualsASCII(orientation, "any"))
    return device::mojom::ScreenOrientationLockType::ANY;
  if (base::LowerCaseEqualsASCII(orientation, "landscape"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE;
  if (base::LowerCaseEqualsASCII(orientation, "portrait"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT;
  if (base::LowerCaseEqualsASCII(orientation, "natural"))
    return device::mojom::ScreenOrientationLockType::NATURAL;
  return device::mojom::ScreenOrientationLockType::DEFAULT;
}

}  // namespace blink
