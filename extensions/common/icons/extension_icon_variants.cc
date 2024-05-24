// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variants.h"

#include "base/values.h"

namespace extensions {

ExtensionIconVariants::ExtensionIconVariants() = default;

ExtensionIconVariants::~ExtensionIconVariants() = default;

ExtensionIconVariants::ExtensionIconVariants(ExtensionIconVariants&& other) =
    default;

// TODO(crbug.com/41419485): Include `warning` in addition to `error`.
bool ExtensionIconVariants::Parse(const base::Value::List* list,
                                  std::u16string* error) {
  // Parse each icon variant in `icon_variants`.
  for (auto& entry : *list) {
    std::string issue;
    auto icon_variant = ExtensionIconVariant::Parse(entry, &issue);
    if (!icon_variant.has_value()) {
      continue;
    }
    list_.emplace_back(std::move(icon_variant.value()));
  }

  // TODO(crbug.com/41419485): Optionally warn if `list_ == 0`, but don't error.
  return true;
}

bool ExtensionIconVariants::IsValid() const {
  return list_.size() > 0;
}

}  //  namespace extensions
