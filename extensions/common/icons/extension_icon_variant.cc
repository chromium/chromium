// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variant.h"

#include <optional>
#include <vector>

#include "extensions/common/manifest_handler_helpers.h"

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

  sizes_[size.value()] = entry.second.GetString();
}

std::unique_ptr<ExtensionIconVariant> ExtensionIconVariant::Parse(
    const base::Value& value,
    std::string* issue) {
  if (!value.is_dict()) {
    return nullptr;
  }

  auto& dict = value.GetDict();
  std::unique_ptr<ExtensionIconVariant> icon_variant =
      std::make_unique<ExtensionIconVariant>();
  for (const auto entry : dict) {
    // `any`. Optional string.
    if (entry.first == "any") {
      icon_variant->any_ = std::make_optional(entry.second.GetString());
      continue;
    }

    // `color_scheme`. Optional string or list of strings.
    if (entry.first == "color_schemes") {
      icon_variant->MaybeAddColorSchemes(entry.second);
      continue;
    }

    // Assume that `entry.first` is an `int` from this point.
    icon_variant->MaybeAddSizeEntry(entry);
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
