// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variant.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handler_helpers.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Convert a string representation of a `"color_scheme"` to an enum value.
std::optional<ExtensionIconVariant::ColorScheme> MaybeGetColorScheme(
    const std::string& color_scheme) {
  if (color_scheme == "dark") {
    return ExtensionIconVariant::ColorScheme::kDark;
  }

  if (color_scheme == "light") {
    return ExtensionIconVariant::ColorScheme::kLight;
  }

  return std::nullopt;
}

}  // namespace

ExtensionIconVariant::ExtensionIconVariant() = default;

ExtensionIconVariant::~ExtensionIconVariant() = default;

ExtensionIconVariant::ExtensionIconVariant(ExtensionIconVariant&& other) =
    default;

ExtensionIconVariant::ExtensionIconVariant(const ExtensionIconVariant& other) =
    default;

bool ExtensionIconVariant::ValidateIconPath(const ExtensionResource& path) {
  if (path.empty()) {
    diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Id::kIconVariantPathInvalid));
    return false;
  }

  if (!manifest_handler_helpers::IsIconMimeTypeValid(path.relative_path())) {
    diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Id::kIconVariantsInvalidMimeType));
    return false;
  }

  return true;
}

// Add color schemes if the input value is valid and has valid color_schemes.
void ExtensionIconVariant::MaybeAddColorSchemes(const base::Value& value) {
  // `value` should be a list. Otherwise add a warning and return.
  if (!value.is_list()) {
    diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Id::kIconVariantColorSchemesType));
    return;
  }

  for (const auto& item : value.GetList()) {
    // Ignore invalid types.
    if (!item.is_string()) {
      diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
          diagnostics::icon_variants::Feature::kIconVariants,
          diagnostics::icon_variants::Id::kIconVariantColorSchemesType));
      continue;
    }

    // A valid `color_scheme` is required.
    auto color_scheme = MaybeGetColorScheme(item.GetString());
    if (!color_scheme.has_value()) {
      diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
          diagnostics::icon_variants::Feature::kIconVariants,
          diagnostics::icon_variants::Id::kIconVariantColorSchemeInvalid));
      continue;
    }

    // Add color_scheme to the list.
    color_schemes_.insert(color_scheme.value());
  }
}

void ExtensionIconVariant::MaybeAddSizeEntry(
    const Extension& extension,
    const std::pair<const std::string&, const base::Value&>& entry) {
  // Get <number> keys if they exist.
  std::optional<int> size =
      manifest_handler_helpers::LoadValidSizeFromString(entry.first);

  // Verify that size is of type `int`.
  if (!size.has_value()) {
    diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Id::kIconVariantSizeInvalid));
    return;
  }

  ExtensionResource icon_path = extension.GetResource(entry.second.GetString());
  if (!ValidateIconPath(icon_path)) {
    return;
  }

  sizes_[size.value()] = std::move(icon_path);
}

std::unique_ptr<ExtensionIconVariant> ExtensionIconVariant::Parse(
    const Extension& extension,
    const base::Value& value) {
  if (!value.is_dict()) {
    return nullptr;
  }

  auto& dict = value.GetDict();
  std::unique_ptr<ExtensionIconVariant> icon_variant =
      std::make_unique<ExtensionIconVariant>();
  for (const auto entry : dict) {
    // `any`. Optional string.
    if (entry.first == "any") {
      ExtensionResource icon_path =
          extension.GetResource(entry.second.GetString());
      if (icon_variant->ValidateIconPath(icon_path)) {
        icon_variant->any_ = std::make_optional(std::move(icon_path));
      }
      continue;
    }

    // `color_scheme`. Optional string or list of strings.
    if (entry.first == "color_schemes") {
      icon_variant->MaybeAddColorSchemes(entry.second);
      continue;
    }

    // Assume that `entry.first` is an `int` from this point.
    icon_variant->MaybeAddSizeEntry(extension, entry);
  }

  if (!icon_variant->IsValid()) {
    return nullptr;
  }

  return icon_variant;
}

bool ExtensionIconVariant::IsValid() const {
  return any_.has_value() || !sizes_.empty();
}

}  // namespace extensions
