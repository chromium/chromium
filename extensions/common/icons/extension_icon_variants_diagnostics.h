// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_
#define EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_

#include <optional>

// A diagnostic is a unique error/warning code which can be retrieved keyed on
// the provided code and feature. An example of a feature is kIconVariants,
// which is an enum entry.
// TODO(crbug.com/343748805): Generalize for features other than icon_variants.
// TODO(crbug.com/343748805): Consider names other than `category` and
// `feature`.
// TODO(crbug.com/343748805): Should `code` be `id instead?
namespace extensions::diagnostics::icon_variants {

// Add a unique name at the bottom of the list and do no sort nor change the
// order. Each id is unique and should remain unchanged.
enum class Code {
  kFailedToParse,
  kIconVariantsEmpty,
  kEmptyIconVariant,
};

// Warning or error?
enum class Severity {
  kError,
  kWarning,
};

// Manifest or API.
enum class Category {
  kManifest,
  kApi,
};

// Support different manifest keys and APIs (aka features) that have
// diagnostics.
enum class Feature {
  kIconVariants,
};

// Retrieval of diagnostic with relevant information.
struct Diagnostic {
  Feature feature;
  icon_variants::Code code;
  Category category;
  Severity severity;
  const char* message;
};

// Get diagnostic for a given id.
Diagnostic GetDiagnosticForID(Feature feature, Code code);

}  // namespace extensions::diagnostics::icon_variants

#endif  // EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_DIAGNOSTICS_H_
