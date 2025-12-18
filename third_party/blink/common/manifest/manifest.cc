// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"

namespace blink {

Manifest::ImageResource::ImageResource() = default;

Manifest::ImageResource::ImageResource(const ImageResource& other) = default;

Manifest::ImageResource::~ImageResource() = default;

bool Manifest::ImageResource::operator==(
    const Manifest::ImageResource& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.src, item.type, item.sizes);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShortcutItem::ShortcutItem() = default;

Manifest::ShortcutItem::~ShortcutItem() = default;

bool Manifest::ShortcutItem::operator==(const ShortcutItem& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.short_name, item.description, item.url,
                    item.icons, item.icons_localized, item.name_localized,
                    item.short_name_localized, item.description_localized);
  };
  return AsTuple(*this) == AsTuple(other);
}

bool Manifest::FileFilter::operator==(const FileFilter& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.accept);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShareTargetParams::ShareTargetParams() = default;

Manifest::ShareTargetParams::~ShareTargetParams() = default;

bool Manifest::ShareTargetParams::operator==(
    const ShareTargetParams& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.title, item.text, item.url, item.files);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShareTarget::ShareTarget() = default;

Manifest::ShareTarget::~ShareTarget() = default;

bool Manifest::ShareTarget::operator==(const ShareTarget& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.action, item.method, item.enctype, item.params);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::RelatedApplication::RelatedApplication() = default;

Manifest::RelatedApplication::~RelatedApplication() = default;

bool Manifest::RelatedApplication::operator==(
    const RelatedApplication& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.platform, item.url, item.id);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::LaunchHandler::LaunchHandler() = default;

Manifest::LaunchHandler::LaunchHandler(std::optional<ClientMode> client_mode)
    : client_mode_(client_mode) {}

// See https://wicg.github.io/web-app-launch/#dfn-process-the-client_mode-member
// for more details.
Manifest::LaunchHandler::ClientMode
Manifest::LaunchHandler::parsed_client_mode() const {
  return client_mode_.value_or(Manifest::LaunchHandler::ClientMode::kAuto);
}

bool Manifest::LaunchHandler::client_mode_valid_and_specified() const {
  return client_mode_.has_value();
}

bool Manifest::LaunchHandler::operator==(const LaunchHandler& other) const {
  return parsed_client_mode() == other.parsed_client_mode();
}

bool Manifest::LaunchHandler::TargetsExistingClients() const {
  switch (parsed_client_mode()) {
    case ClientMode::kAuto:
    case ClientMode::kNavigateNew:
      return false;
    case ClientMode::kNavigateExisting:
    case ClientMode::kFocusExisting:
      return true;
  }
}

bool Manifest::LaunchHandler::NeverNavigateExistingClients() const {
  switch (parsed_client_mode()) {
    case ClientMode::kAuto:
    case ClientMode::kNavigateNew:
    case ClientMode::kNavigateExisting:
      return false;
    case ClientMode::kFocusExisting:
      return true;
  }
}

Manifest::TranslationItem::TranslationItem() = default;

Manifest::TranslationItem::~TranslationItem() = default;

bool Manifest::TranslationItem::operator==(const TranslationItem& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.short_name, item.description);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::HomeTabParams::HomeTabParams() = default;

Manifest::HomeTabParams::~HomeTabParams() = default;

bool Manifest::HomeTabParams::operator==(const HomeTabParams& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.icons, item.scope_patterns);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::NewTabButtonParams::NewTabButtonParams() = default;

Manifest::NewTabButtonParams::~NewTabButtonParams() = default;

bool Manifest::NewTabButtonParams::operator==(
    const NewTabButtonParams& other) const {
  return url == other.url;
}

Manifest::TabStrip::TabStrip() = default;

Manifest::TabStrip::~TabStrip() = default;

bool Manifest::TabStrip::operator==(const TabStrip& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.home_tab, item.new_tab_button);
  };
  return AsTuple(*this) == AsTuple(other);
}

// static
Manifest::DisplayOverride Manifest::DisplayOverride::Create(
    blink::mojom::DisplayMode display_mode) {
  return DisplayOverride(display_mode, {});
}

// static
Manifest::DisplayOverride Manifest::DisplayOverride::CreateUnframed(
    std::vector<SafeUrlPattern> url_patterns) {
  return DisplayOverride(blink::mojom::DisplayMode::kBorderless,
                         std::move(url_patterns));
}

Manifest::DisplayOverride::DisplayOverride(
    blink::mojom::DisplayMode display_mode,
    std::vector<SafeUrlPattern> patterns)
    : display_(display_mode), url_patterns_(std::move(patterns)) {
  CHECK(url_patterns_.empty() ||
        display_ == blink::mojom::DisplayMode::kBorderless)
      << "url_patterns is not allowed in display modes other than 'unframed'";
}

Manifest::DisplayOverride::DisplayOverride() = default;
Manifest::DisplayOverride::DisplayOverride(const DisplayOverride& other) =
    default;
Manifest::DisplayOverride::DisplayOverride(DisplayOverride&& other) = default;
Manifest::DisplayOverride& Manifest::DisplayOverride::operator=(
    const DisplayOverride& other) = default;
Manifest::DisplayOverride& Manifest::DisplayOverride::operator=(
    DisplayOverride&& other) = default;
Manifest::DisplayOverride::~DisplayOverride() = default;

bool Manifest::DisplayOverride::operator==(const DisplayOverride& other) const =
    default;

}  // namespace blink
