// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_
#define EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/icons/extension_icon_variant.h"
#include "extensions/common/icons/extension_icon_variants_diagnostics.h"

namespace extensions {

// Representation of the `icon_variants` key anywhere in manifest.json. It could
// be a top level key or a subkey of `action`.
class ExtensionIconVariants {
 public:
  ExtensionIconVariants();
  ExtensionIconVariants(const ExtensionIconVariants& other) = delete;
  ExtensionIconVariants(ExtensionIconVariants&& other);
  ~ExtensionIconVariants();

  // Parse the provided list from manifest.json and set `list_` with the result.
  bool Parse(const base::Value::List* list,
             std::vector<diagnostics::icon_variants::Diagnostic>& diagnostics);

  // Determine whether `list_` has at least one icon variant after parsing.
  bool IsEmpty() const;

 private:
  std::vector<ExtensionIconVariant> list_;
};

}  //  namespace extensions

#endif  // EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_
