// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/icon_variants_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/icon_variants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/icons/extension_icon_variants.h"
#include "extensions/common/manifest_constants.h"
#include "ui/gfx/color_utils.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {
static base::LazyInstance<ExtensionIconSet>::DestructorAtExit g_empty_icon_set =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace keys = manifest_keys;

IconVariantsInfo::IconVariantsInfo() = default;
IconVariantsInfo::~IconVariantsInfo() = default;
IconVariantsHandler::IconVariantsHandler() = default;
IconVariantsHandler::~IconVariantsHandler() = default;

using extensions::api::icon_variants::ManifestKeys;
using extensions::diagnostics::icon_variants::Id;
using extensions::diagnostics::icon_variants::Severity;
using extensions::diagnostics::icon_variants::Feature;

namespace {
void AddInstallWarning(Extension& extension, const std::string& warning) {
  extension.AddInstallWarning(InstallWarning(warning));
}

void AddInstallWarningForId(Extension& extension, Id id) {
  auto diagnostic = extensions::diagnostics::icon_variants::GetDiagnostic(
      Feature::kIconVariants, id);
  if (diagnostic.severity != Severity::kWarning) {
    return;
  }
  AddInstallWarning(extension, diagnostic.message);
}

// Returns the icon variants parsed from the `extension` manifest.
// Populates `error` if there are no icon variants.
ExtensionIconVariants GetIconVariants(Extension& extension) {
  ExtensionIconVariants icon_variants;

  // Convert the input key into a list containing everything.
  const base::Value::List* icon_variants_list =
      extension.manifest()->available_values().FindList(keys::kIconVariants);
  if (!icon_variants_list) {
    icon_variants.AddDiagnostic(Feature::kIconVariants,
                                Id::kIconVariantsKeyMustBeAList);
    return icon_variants;
  }

  icon_variants.Parse(extension, icon_variants_list);

  // Verify `icon_variants`, e.g. that at least one `icon_variant` is valid.
  if (icon_variants.IsEmpty()) {
    icon_variants.AddDiagnostic(Feature::kIconVariants,
                                Id::kIconVariantsInvalid);
  }

  return icon_variants;
}
}  // namespace

// static
bool IconVariantsInfo::HasIconVariants(const Extension* extension) {
  DCHECK(extension);
  if (!IconVariantsInfo::SupportsIconVariants(*extension)) {
    return false;
  }
  const IconVariantsInfo* info = IconVariantsInfo::GetIconVariants(*extension);
  return info && info->icon_variants && !info->icon_variants->IsEmpty();
}

// static
const IconVariantsInfo* IconVariantsInfo::GetIconVariants(
    const Extension& extension) {
  if (!IconVariantsInfo::SupportsIconVariants(extension)) {
    return nullptr;
  }
  return static_cast<IconVariantsInfo*>(
      extension.GetManifestData(keys::kIconVariants));
}

// static
bool IconVariantsInfo::SupportsIconVariants(const Extension& extension) {
  if (extension.manifest_version() < 3 || !extension.is_extension()) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      extensions_features::kExtensionIconVariants);
}

void IconVariantsInfo::InitializeIconSets() {
  for (const auto& icon_variant : icon_variants->GetList()) {
    const auto color_schemes = icon_variant.GetColorSchemes();
    const auto sizes = icon_variant.GetSizes();
    // TODO(crbug.com/344639840): Support any, e.g. any = icon_variant.GetAny();

    for (const auto& size : sizes) {
      // Add the size path pair to both extension icon sets if unspecified.
      if (color_schemes.empty()) {
        dark_.Add(size.first, size.second.relative_path().AsUTF8Unsafe());
        light_.Add(size.first, size.second.relative_path().AsUTF8Unsafe());
        continue;
      }

      if (color_schemes.contains(ExtensionIconVariant::ColorScheme::kDark)) {
        dark_.Add(size.first, size.second.relative_path().AsUTF8Unsafe());
      }

      if (color_schemes.contains(ExtensionIconVariant::ColorScheme::kLight)) {
        light_.Add(size.first, size.second.relative_path().AsUTF8Unsafe());
      }
    }
  }
}

const ExtensionIconSet& IconVariantsInfo::Get(
    std::optional<ExtensionIconVariant::ColorScheme> color_scheme) const {
  if (!icon_variants) {
    g_empty_icon_set.Get();
  }

  return color_scheme == ExtensionIconVariant::ColorScheme::kDark ? dark_
                                                                  : light_;
}

bool IconVariantsHandler::Parse(Extension* extension, std::u16string* error) {
  DCHECK(extension);

  if (!IconVariantsInfo::SupportsIconVariants(*extension)) {
    AddInstallWarningForId(*extension, Id::kIconVariantsNotEnabled);
    return true;
  }

  // The `icon_variants` key should be able to be parsed from generated .idl.
  // This only verifies the limited subset of keys supported by
  // json_schema_compiler. The manifest_keys wouldn't contain icon sizes, so
  // all keys will be parsed from the same source list after this verification.
  std::u16string ignore_generated_parsing_errors;
  ManifestKeys manifest_keys;
  if (!ManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys,
          ignore_generated_parsing_errors)) {
    // `ParseFromDictionary` returns false if .e.g. a manifest string doesn't
    // match an .idl enum or a dictionary value type doesn't match .idl.
    // Don't return false on error to allow for non-breaking changes later on.
    AddInstallWarningForId(*extension, Id::kFailedToParse);
  }

  // If `ExtensionIconVariants` isn't returned, it's ok to just show an error
  // and ignore possible warnings.
  ExtensionIconVariants icon_variants = GetIconVariants(*extension);

  // Add any install warnings, handle errors, and then clear out diagnostics.
  auto& diagnostics = icon_variants.get_diagnostics();
  for (auto& diagnostic : diagnostics) {
    // If any error exists, do not load the extension.
    if (diagnostic.severity == diagnostics::icon_variants::Severity::kError) {
      *error = base::UTF8ToUTF16(diagnostic.message);
      return false;
    }

    AddInstallWarningForId(*extension, diagnostic.id);
  }
  diagnostics.clear();

  // Save the result in the info object.
  std::unique_ptr<IconVariantsInfo> icon_variants_info(
      std::make_unique<IconVariantsInfo>());
  icon_variants_info->icon_variants = std::move(icon_variants);
  icon_variants_info->InitializeIconSets();

  extension->SetManifestData(keys::kIconVariants,
                             std::move(icon_variants_info));
  return true;
}

bool IconVariantsHandler::Validate(
    const Extension& extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // TODO(crbug.com/41419485): Validate icon existence.
  return true;
}

base::span<const char* const> IconVariantsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kIconVariants};
  return kKeys;
}

}  // namespace extensions
