// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/manifest/capture_links.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace blink {

bool IsEmptyManifest(const mojom::Manifest& manifest) {
  static base::NoDestructor<mojom::ManifestPtr> empty_manifest_ptr_storage;
  mojom::ManifestPtr& empty_manifest = *empty_manifest_ptr_storage;
  if (!empty_manifest)
    empty_manifest = mojom::Manifest::New();
  return manifest == *empty_manifest;
}

bool IsEmptyManifest(const mojom::ManifestPtr& manifest) {
  return !manifest || IsEmptyManifest(*manifest);
}

bool IsDefaultManifest(const mojom::Manifest& manifest,
                       const GURL& document_url) {
  blink::mojom::ManifestPtr expected_manifest = blink::mojom::Manifest::New();
  expected_manifest->start_url = document_url;
  expected_manifest->id = document_url.GetWithoutRef();
  expected_manifest->scope = document_url.GetWithoutFilename();
  return manifest == *expected_manifest;
}

bool IsDefaultManifest(const mojom::ManifestPtr& manifest,
                       const GURL& document_url) {
  return manifest && IsDefaultManifest(*manifest, document_url);
}

std::optional<blink::mojom::Manifest_TextDirection> TextDirectionFromString(
    const std::string& dir) {
  using TextDirection = blink::mojom::Manifest_TextDirection;
  if (base::EqualsCaseInsensitiveASCII(dir, "auto")) {
    return TextDirection::kAuto;
  }
  if (base::EqualsCaseInsensitiveASCII(dir, "ltr")) {
    return TextDirection::kLTR;
  }
  if (base::EqualsCaseInsensitiveASCII(dir, "rtl")) {
    return TextDirection::kRTL;
  }
  return std::nullopt;
}

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
    case blink::mojom::DisplayMode::kTabbed:
      return "tabbed";
    case blink::mojom::DisplayMode::kBorderless:
      return "borderless";
    case blink::mojom::DisplayMode::kPictureInPicture:
      return "picture-in-picture";
  }
  return "";
}

blink::mojom::DisplayMode DisplayModeFromString(const std::string& display) {
  if (base::EqualsCaseInsensitiveASCII(display, "browser"))
    return blink::mojom::DisplayMode::kBrowser;
  if (base::EqualsCaseInsensitiveASCII(display, "minimal-ui"))
    return blink::mojom::DisplayMode::kMinimalUi;
  if (base::EqualsCaseInsensitiveASCII(display, "standalone"))
    return blink::mojom::DisplayMode::kStandalone;
  if (base::EqualsCaseInsensitiveASCII(display, "fullscreen"))
    return blink::mojom::DisplayMode::kFullscreen;
  if (base::EqualsCaseInsensitiveASCII(display, "window-controls-overlay"))
    return blink::mojom::DisplayMode::kWindowControlsOverlay;
  if (base::EqualsCaseInsensitiveASCII(display, "tabbed"))
    return blink::mojom::DisplayMode::kTabbed;
  if (base::EqualsCaseInsensitiveASCII(display, "borderless"))
    return blink::mojom::DisplayMode::kBorderless;
  if (base::EqualsCaseInsensitiveASCII(display, "picture-in-picture")) {
    return blink::mojom::DisplayMode::kPictureInPicture;
  }
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
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait-primary"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait-secondary"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY;
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape-primary"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape-secondary"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY;
  if (base::EqualsCaseInsensitiveASCII(orientation, "any"))
    return device::mojom::ScreenOrientationLockType::ANY;
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape"))
    return device::mojom::ScreenOrientationLockType::LANDSCAPE;
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait"))
    return device::mojom::ScreenOrientationLockType::PORTRAIT;
  if (base::EqualsCaseInsensitiveASCII(orientation, "natural"))
    return device::mojom::ScreenOrientationLockType::NATURAL;
  return device::mojom::ScreenOrientationLockType::DEFAULT;
}

mojom::CaptureLinks CaptureLinksFromString(const std::string& capture_links) {
  if (base::EqualsCaseInsensitiveASCII(capture_links, "none"))
    return mojom::CaptureLinks::kNone;
  if (base::EqualsCaseInsensitiveASCII(capture_links, "new-client"))
    return mojom::CaptureLinks::kNewClient;
  if (base::EqualsCaseInsensitiveASCII(capture_links,
                                       "existing-client-navigate"))
    return mojom::CaptureLinks::kExistingClientNavigate;
  return mojom::CaptureLinks::kUndefined;
}

std::optional<mojom::ManifestLaunchHandler::ClientMode> ClientModeFromString(
    const std::string& client_mode) {
  using ClientMode = Manifest::LaunchHandler::ClientMode;
  if (base::EqualsCaseInsensitiveASCII(client_mode, "auto"))
    return ClientMode::kAuto;
  if (base::EqualsCaseInsensitiveASCII(client_mode, "navigate-new"))
    return ClientMode::kNavigateNew;
  if (base::EqualsCaseInsensitiveASCII(client_mode, "navigate-existing"))
    return ClientMode::kNavigateExisting;
  if (base::EqualsCaseInsensitiveASCII(client_mode, "focus-existing"))
    return ClientMode::kFocusExisting;
  return std::nullopt;
}

}  // namespace blink
