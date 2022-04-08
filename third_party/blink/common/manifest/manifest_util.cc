// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_util.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
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
  if (base::LowerCaseEqualsASCII(display, "tabbed"))
    return blink::mojom::DisplayMode::kTabbed;
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

mojom::CaptureLinks CaptureLinksFromString(const std::string& capture_links) {
  if (base::LowerCaseEqualsASCII(capture_links, "none"))
    return mojom::CaptureLinks::kNone;
  if (base::LowerCaseEqualsASCII(capture_links, "new-client"))
    return mojom::CaptureLinks::kNewClient;
  if (base::LowerCaseEqualsASCII(capture_links, "existing-client-navigate"))
    return mojom::CaptureLinks::kExistingClientNavigate;
  return mojom::CaptureLinks::kUndefined;
}

mojom::HandleLinks HandleLinksFromString(const std::string& handle_links) {
  if (base::LowerCaseEqualsASCII(handle_links, "auto"))
    return mojom::HandleLinks::kAuto;
  if (base::LowerCaseEqualsASCII(handle_links, "preferred"))
    return mojom::HandleLinks::kPreferred;
  if (base::LowerCaseEqualsASCII(handle_links, "not-preferred"))
    return mojom::HandleLinks::kNotPreferred;
  return mojom::HandleLinks::kUndefined;
}

bool ParsedRouteTo::operator==(const ParsedRouteTo& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.route_to, item.legacy_existing_client_value);
  };
  return AsTuple(*this) == AsTuple(other);
}

bool ParsedRouteTo::operator!=(const ParsedRouteTo& other) const {
  return !(*this == other);
}

absl::optional<ParsedRouteTo> RouteToFromString(const std::string& route_to) {
  using RouteTo = Manifest::LaunchHandler::RouteTo;
  if (base::LowerCaseEqualsASCII(route_to, "auto"))
    return ParsedRouteTo{.route_to = RouteTo::kAuto};
  if (base::LowerCaseEqualsASCII(route_to, "new-client"))
    return ParsedRouteTo{.route_to = RouteTo::kNewClient};
  if (base::LowerCaseEqualsASCII(route_to, "existing-client"))
    return ParsedRouteTo{.legacy_existing_client_value = true};
  if (base::LowerCaseEqualsASCII(route_to, "existing-client-navigate"))
    return ParsedRouteTo{.route_to = RouteTo::kExistingClientNavigate};
  if (base::LowerCaseEqualsASCII(route_to, "existing-client-retain"))
    return ParsedRouteTo{.route_to = RouteTo::kExistingClientRetain};
  return absl::nullopt;
}

absl::optional<NavigateExistingClient> NavigateExistingClientFromString(
    const std::string& navigate_existing_client) {
  if (base::LowerCaseEqualsASCII(navigate_existing_client, "always"))
    return NavigateExistingClient::kAlways;
  if (base::LowerCaseEqualsASCII(navigate_existing_client, "never"))
    return NavigateExistingClient::kNever;
  return absl::nullopt;
}

}  // namespace blink
