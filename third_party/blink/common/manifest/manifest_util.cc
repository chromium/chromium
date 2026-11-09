// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace blink {

bool IsDefaultManifest(const Manifest& manifest, const GURL& document_url) {
  auto default_manifest = Manifest::CreateDefault(document_url);
  return default_manifest.has_value() && manifest == *default_manifest;
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

std::string TextDirectionToString(
    blink::mojom::Manifest_TextDirection direction) {
  switch (direction) {
    case blink::mojom::Manifest_TextDirection::kAuto:
      return "auto";
    case blink::mojom::Manifest_TextDirection::kLTR:
      return "ltr";
    case blink::mojom::Manifest_TextDirection::kRTL:
      return "rtl";
  }
  NOTREACHED();
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
    case blink::mojom::DisplayMode::kUnframed:
      return "unframed";
    case blink::mojom::DisplayMode::kPictureInPicture:
      return "picture-in-picture";
  }
  NOTREACHED();
}

blink::mojom::DisplayMode DisplayModeFromString(const std::string& display) {
  if (base::EqualsCaseInsensitiveASCII(display, "browser")) {
    return blink::mojom::DisplayMode::kBrowser;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "minimal-ui")) {
    return blink::mojom::DisplayMode::kMinimalUi;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "standalone")) {
    return blink::mojom::DisplayMode::kStandalone;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "fullscreen")) {
    return blink::mojom::DisplayMode::kFullscreen;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "window-controls-overlay")) {
    return blink::mojom::DisplayMode::kWindowControlsOverlay;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "tabbed")) {
    return blink::mojom::DisplayMode::kTabbed;
  }
  if (base::EqualsCaseInsensitiveASCII(display, "unframed")) {
    return blink::mojom::DisplayMode::kUnframed;
  }
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
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait-primary")) {
    return device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait-secondary")) {
    return device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape-primary")) {
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape-secondary")) {
    return device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "any")) {
    return device::mojom::ScreenOrientationLockType::ANY;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "landscape")) {
    return device::mojom::ScreenOrientationLockType::LANDSCAPE;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "portrait")) {
    return device::mojom::ScreenOrientationLockType::PORTRAIT;
  }
  if (base::EqualsCaseInsensitiveASCII(orientation, "natural")) {
    return device::mojom::ScreenOrientationLockType::NATURAL;
  }
  return device::mojom::ScreenOrientationLockType::DEFAULT;
}

std::optional<mojom::ManifestLaunchHandler::ClientMode> ClientModeFromString(
    const std::string& client_mode) {
  using ClientMode = Manifest::LaunchHandler::ClientMode;
  if (base::EqualsCaseInsensitiveASCII(client_mode, "auto")) {
    return ClientMode::kAuto;
  }
  if (base::EqualsCaseInsensitiveASCII(client_mode, "navigate-new")) {
    return ClientMode::kNavigateNew;
  }
  if (base::EqualsCaseInsensitiveASCII(client_mode, "navigate-existing")) {
    return ClientMode::kNavigateExisting;
  }
  if (base::EqualsCaseInsensitiveASCII(client_mode, "focus-existing")) {
    return ClientMode::kFocusExisting;
  }
  return std::nullopt;
}

bool IsValidFileHandlerMimeType(std::string_view mime_type) {
  std::string top_level_mime_type;
  std::string subtype;
  // `net::ParseMimeTypeWithoutParameter()` trims leading and trailing ASCII
  // whitespace before splitting on '/', so also verify that the parsed
  // `top_level_mime_type` and `subtype` (plus the '/' separator) account for
  // the entire input `mime_type` with no surrounding whitespace.
  return net::ParseMimeTypeWithoutParameter(mime_type, &top_level_mime_type,
                                            &subtype) &&
         top_level_mime_type.size() + 1 + subtype.size() == mime_type.size() &&
         net::IsValidTopLevelMimeType(top_level_mime_type) &&
         !base::StartsWith(top_level_mime_type, "x-",
                           base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace blink
