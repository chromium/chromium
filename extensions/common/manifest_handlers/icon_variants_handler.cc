// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/icon_variants_handler.h"

#include <memory>
#include <string>

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
using Code = extensions::diagnostics::icon_variants::Code;
using Severity = extensions::diagnostics::icon_variants::Severity;
using Feature = extensions::diagnostics::icon_variants::Feature;

namespace {
void AddInstallWarning(Extension& extension, const std::string& warning) {
  extension.AddInstallWarning(InstallWarning(warning));
}

void AddInstallWarningForCode(Extension& extension, Code code) {
  auto diagnostic = extensions::diagnostics::icon_variants::GetDiagnosticForID(
      Feature::kIconVariants, code);
  if (diagnostic.severity != Severity::kWarning) {
    return;
  }
  AddInstallWarning(extension, diagnostic.message);
}
}  // namespace

// static
bool IconVariantsInfo::HasIconVariants(const Extension* extension) {
  DCHECK(extension);
  const IconVariantsInfo* info = GetIconVariants(extension);
  return info && info->icon_variants;
}

const IconVariantsInfo* IconVariantsInfo::GetIconVariants(
    const Extension* extension) {
  return static_cast<IconVariantsInfo*>(
      extension->GetManifestData(IconVariantsManifestKeys::kIconVariants));
}

bool IconVariantsHandler::Parse(Extension* extension, std::u16string* error) {
  DCHECK(extension);

  // The `icon_variants` key should be able to be parsed from generated .idl.
  // This only verifies the limited subset of keys supported by
  // json_schema_compiler. The manifest_keys wouldn't contain icon sizes, so
  // all keys will be parsed from the same source list after this verification.
  //
  // Don't return false on error. `DOMString` for `color_scheme` in .idl
  // wouldn't cause a parse error, but e.g. `enum` does. Therefore those will be
  // treated as warnings.
  std::u16string ignore_generated_parsing_errors;
  IconVariantsManifestKeys manifest_keys;
  if (!IconVariantsManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys,
          ignore_generated_parsing_errors)) {
    // TODO(crbug.com/41419485): Maybe emit `warning`. A problem is that the
    // .idl parser returns false if manifest value doesn't match an .idl enum,
    // but `warning` is empty in that case.
    AddInstallWarningForCode(*extension, Code::kFailedToParse);
  }

  // Convert the input key into a list containing everything.
  const base::Value::List* icon_variants_list =
      extension->manifest()->available_values().FindList(keys::kIconVariants);
  if (!icon_variants_list) {
    // TODO(crbug.com/41419485): Specific error that the value isn't a list.
    *error = manifest_errors::kInvalidIconVariants;
    return false;
  }

  // Parse the `icon_variants` key.
  std::unique_ptr<ExtensionIconVariants> icon_variants =
      std::make_unique<ExtensionIconVariants>();
  // TODO(crbug.com/344639840): Consider moving icon_variant* impl here to avoid
  // bubbling up warnings and errors.
  std::vector<diagnostics::icon_variants::Diagnostic> diagnostics;
  if (!icon_variants->Parse(icon_variants_list, diagnostics)) {
    AddInstallWarningForCode(*extension, Code::kFailedToParse);
    // TODO(crbug.com/41419485): Use the WECG proposal to determine warn/error.
    return true;
  }

  // If there are any parse warnings, add them to the install warnings.
  for (const auto& diagnostic : diagnostics) {
    AddInstallWarningForCode(*extension, diagnostic.code);
  }

  // Verify `icon_variants`, e.g. that at least one `icon_variant` is valid.
  // TODO(crbug.com/344639840): Consider whether an empty list should be an
  // error or just a warning instead.
  if (icon_variants->IsEmpty()) {
    *error = std::u16string(u"Error: Invalid icon_variants.");
    return false;
  }

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
