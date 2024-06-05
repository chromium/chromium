// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variants_diagnostics.h"

#include <optional>

namespace extensions::diagnostics::icon_variants {

// Add new diagnostics here.
Diagnostic diagnostics[] = {
    {
        Feature::kIconVariants,
        Code::kFailedToParse,
        Category::kManifest,
        Severity::kWarning,
        "Failed to parse.",
    },
    {
        Feature::kIconVariants,
        Code::kIconVariantsEmpty,
        Category::kManifest,
        Severity::kWarning,
        "There are no usable icon_variants.",
    },
    {
        Feature::kIconVariants,
        Code::kEmptyIconVariant,
        Category::kManifest,
        Severity::kWarning,
        "Icon variant is empty.",
    },
};

// TODO(crbug.com/343748805): Use e.g. flat_map when there are many diagnostics.
Diagnostic GetDiagnosticForID(Feature feature, Code code) {
  for (const auto& diagnostic : diagnostics) {
    if (diagnostic.feature == feature && diagnostic.code == code) {
      return diagnostic;
    }
  }

  // A diagnostic match should always be found before reaching this point.
  return Diagnostic();
}

}  // namespace extensions::diagnostics::icon_variants
