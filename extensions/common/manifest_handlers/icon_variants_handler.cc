// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/icon_variants_handler.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/icon_variants.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_variants.h"
#include "extensions/common/icons/extension_icon_variants_diagnostics.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

IconVariantsInfo::IconVariantsInfo() = default;
IconVariantsInfo::~IconVariantsInfo() = default;

IconVariantsHandler::IconVariantsHandler() = default;
IconVariantsHandler::~IconVariantsHandler() = default;

namespace keys = manifest_keys;
using IconVariantsManifestKeys = extensions::api::icon_variants::ManifestKeys;

// extensions::diagnostics::
using Id = extensions::diagnostics::icon_variants::Id;
using Severity = extensions::diagnostics::icon_variants::Severity;
using Feature = extensions::diagnostics::icon_variants::Feature;

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
std::unique_ptr<ExtensionIconVariants> GetIconVariants(Extension& extension,
                                                       std::u16string* error) {
  // Convert the input key into a list containing everything.
  const base::Value::List* icon_variants_list =
      extension.manifest()->available_values().FindList(keys::kIconVariants);
  if (!icon_variants_list) {
    *error = base::UTF8ToUTF16(
        diagnostics::icon_variants::GetDiagnostic(
            Feature::kIconVariants, Id::kIconVariantsKeyMustBeAList)
            .message);
    return nullptr;
  }

  std::vector<diagnostics::icon_variants::Diagnostic> diagnostics;

  std::unique_ptr<ExtensionIconVariants> icon_variants =
      std::make_unique<ExtensionIconVariants>();
  icon_variants->Parse(icon_variants_list);

  // Verify `icon_variants`, e.g. that at least one `icon_variant` is valid.
  // TODO(crbug.com/344639840): Consider whether an empty list should be an
  // error or just a warning instead (for future proofing).
  if (icon_variants->IsEmpty()) {
    *error =
        base::UTF8ToUTF16(diagnostics::icon_variants::GetDiagnostic(
                              Feature::kIconVariants, Id::kIconVariantsInvalid)
                              .message);
    return nullptr;
  }

  return icon_variants;
}
}  // namespace

// static
bool IconVariantsInfo::HasIconVariants(const Extension* extension) {
  DCHECK(extension);
  const IconVariantsInfo* info = IconVariantsInfo::GetIconVariants(extension);
  return info && info->icon_variants;
}

const IconVariantsInfo* IconVariantsInfo::GetIconVariants(
    const Extension* extension) {
  return static_cast<IconVariantsInfo*>(
      extension->GetManifestData(IconVariantsManifestKeys::kIconVariants));
}

// TODO(crbug.com/41419485): Add more test coverage for warnings and errors.
bool IconVariantsHandler::Parse(Extension* extension, std::u16string* error) {
  DCHECK(extension);

  // The `icon_variants` key should be able to be parsed from generated .idl.
  // This only verifies the limited subset of keys supported by
  // json_schema_compiler. The manifest_keys wouldn't contain icon sizes, so
  // all keys will be parsed from the same source list after this verification.
  std::u16string ignore_generated_parsing_errors;
  IconVariantsManifestKeys manifest_keys;
  if (!IconVariantsManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys,
          ignore_generated_parsing_errors)) {
    // `ParseFromDictionary` returns false if .e.g. a manifest string doesn't
    // match an .idl enum or a dictionary value type doesn't match .idl.
    // Don't return false on error to allow for non-breaking changes later on.
    AddInstallWarningForId(*extension, Id::kFailedToParse);
  }

  // If `ExtensionIconVariants` isn't returned, it's ok to just show an error
  // and ignore possible warnings.
  // TODO(crbug.com/41419485): Don't set `error` from within `GetIconVariants`.
  // Instead, return with one diagnostic as the error as soon as one is found.
  std::unique_ptr<ExtensionIconVariants> icon_variants =
      GetIconVariants(*extension, error);

  // TODO(crbug.com/41419485): Consider not generating an error this and other
  // cases, unless absolutely necessary. Additionally, consider letting all
  // errors be caught in the handling below, instead of this custom if block.
  if (!icon_variants) {
    // `error` is being populated in `GetIconVariants`.
    return false;
  }

  // Add any install warnings, handle errors, and then clear out diagnostics.
  // TODO(crbug.com/41419485): If there is an error, warnings can be omitted.
  auto& diagnostics = icon_variants->get_diagnostics();
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

  extension->SetManifestData(keys::kIconVariants,
                             std::move(icon_variants_info));
  return true;
}

bool IconVariantsHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // TODO(crbug.com/41419485): Validate icons.
  return true;
}

base::span<const char* const> IconVariantsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kIconVariants};
  return kKeys;
}

}  // namespace extensions
