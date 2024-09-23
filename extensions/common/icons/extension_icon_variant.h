// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANT_H_
#define EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANT_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "extensions/common/icons/extension_icon_variants_diagnostics.h"

namespace extensions {

// Either `any` or `map` must have a non-empty and valid size and/or path.
class ExtensionIconVariant {
 public:
  ExtensionIconVariant();
  ExtensionIconVariant(const ExtensionIconVariant& other) = delete;
  ExtensionIconVariant(ExtensionIconVariant&& other);
  ~ExtensionIconVariant();

  // Options for `"color_scheme"` in the `"icon_variants"` manifest key.
  enum class ColorScheme {
    kDark,
    kLight,
  };

  // Parse the base::value argument and return an instance of this class.
  // TODO(crbug.com/344639840): Remove `issue` and rely on `diagnostics_`?
  static std::unique_ptr<ExtensionIconVariant> Parse(const base::Value& dict,
                                                     std::string* issue);

  std::vector<diagnostics::icon_variants::Diagnostic>& get_diagnostics() {
    return diagnostics_;
  }

 private:
  // Helper methods that add to `this` object if the parameter is valid.
  void MaybeAddColorSchemes(const base::Value& value);
  void MaybeAddSizeEntry(
      const std::pair<const std::string&, const base::Value&>& entry);

  // Either `any` or `<size>` keys must have at least one value.
  bool IsValid() const;

  using Size = short;
  using Path = std::string;

  // The any key can have a path that's for any size.
  std::optional<Path> any_;

  // The color_schemes key can be omitted, or it can be an array with zero or
  // more values.
  std::set<ColorScheme> color_schemes_;

  // Size keys are numbers represented as strings in JSON for which there is no
  // IDL nor json_schema_compiler support.
  base::flat_map<Size, Path> sizes_;

  // Warnings observed while parsing `icon_variants` from manifest.json. These
  // will be cleared at the end of manifest parsing for memory optimization.
  std::vector<diagnostics::icon_variants::Diagnostic> diagnostics_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANT_H_
