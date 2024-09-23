// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variants_diagnostics.h"

#include <optional>

namespace extensions::diagnostics::icon_variants {

// List of diagnostics.
// TODO(crbug.com/344639840): Add a cross-browser code in each item for the UI.
constexpr Diagnostic diagnostics[] = {
    {
        Feature::kIconVariants,
        Id::kFailedToParse,
        Surface::kManifest,
        Severity::kWarning,
        "Failed to parse.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantsEmpty,
        Surface::kManifest,
        Severity::kWarning,
        "There are no usable 'icon_variants'.",
    },
    {
        Feature::kIconVariants,
        Id::kEmptyIconVariant,
        Surface::kManifest,
        Severity::kWarning,
        "Icon variant is empty.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantSizeInvalid,
        Surface::kManifest,
        Severity::kWarning,
        "Icon variant 'size' is not valid.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantSizeInvalid,
        Surface::kManifest,
        Severity::kWarning,
        "Icon variant `color_scheme` is not valid.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantsInvalid,
        Surface::kManifest,
        Severity::kError,
        "Error: 'icon_variants' is not valid.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantsKeyMustBeAList,
        Surface::kManifest,
        Severity::kError,
        "'icon_variants' must be a list.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantColorSchemesType,
        Surface::kManifest,
        Severity::kWarning,
        "Unexpected 'color_schemes' type.",
    },
    {
        Feature::kIconVariants,
        Id::kIconVariantColorSchemeInvalid,
        Surface::kManifest,
        Severity::kWarning,
        "Unexpected 'color_scheme'.",
    },
};

// TODO(crbug.com/343748805): Use e.g. flat_map when there are many diagnostics.
Diagnostic GetDiagnostic(Feature feature, Id id) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.feature == feature && diagnostic.id == id) {
      return diagnostic;
    }
  }

  // A diagnostic match should always be found before reaching this point.
  return Diagnostic();
}

}  // namespace extensions::diagnostics::icon_variants
