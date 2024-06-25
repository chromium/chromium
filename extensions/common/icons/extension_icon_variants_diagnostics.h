// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_
#define EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_

namespace extensions::diagnostics::icon_variants {

// Sorted list of internal ids. Unlike codes, these are not to be surfaced.
enum class Id {
  kEmptyIconVariant,
  kFailedToParse,
  kIconVariantColorSchemeInvalid,
  kIconVariantColorSchemesType,
  kIconVariantsEmpty,
  kIconVariantsInvalid,
  kIconVariantSizeInvalid,
  kIconVariantsKeyMustBeAList,
};

// Represents how significant something is.
enum class Severity {
  // A condition that prevents an extension from loading.
  kError,

  // A condition that does not prevent extension loading, but can be shown.
  kWarning,
};

// A surface represents the location where the diagnostic materialized.
enum class Surface {
  kManifest,
  kApi,
};

// A component of extension capabilities, such as APIs.
enum class Feature {
  kIconVariants,
};

// A collection of metadata accompanied by a message.
// TODO(crbug.com/343748805): Generalize for features other than icon_variants.
struct Diagnostic {
  Feature feature;
  icon_variants::Id id;
  Surface surface;
  Severity severity;
  const char* message;
};

// Get the diagnostic for the given parameters.
Diagnostic GetDiagnostic(Feature feature, Id id);

}  // namespace extensions::diagnostics::icon_variants

#endif  // EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_
