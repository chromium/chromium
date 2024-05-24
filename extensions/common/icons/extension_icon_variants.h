// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_
#define EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/icons/extension_icon_variant.h"

namespace extensions {

// Representation of the `icon_variants` key anywhere in manifest.json. It could
// be a top level key or a subkey of `action`.
class ExtensionIconVariants {
 public:
  ExtensionIconVariants();
  ExtensionIconVariants(const ExtensionIconVariants& other) = delete;
  ExtensionIconVariants(ExtensionIconVariants&& other);
  ~ExtensionIconVariants();

  bool Parse(const base::Value::List* list, std::u16string* error);

  // Verify the `icon_variants` key, e.g. that at least one `icon_variant` is
  // valid.
  bool IsValid() const;

 private:
  std::vector<ExtensionIconVariant> list_;
};

}  //  namespace extensions

#endif  // EXTENSIONS_COMMON_ICONS_EXTENSION_ICON_VARIANTS_H_
