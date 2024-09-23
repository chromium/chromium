// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/icons/extension_icon_variants.h"

#include "base/values.h"
#include "extensions/common/icons/extension_icon_variants_diagnostics.h"

namespace extensions {

namespace {
using Diagnostic = diagnostics::icon_variants::Diagnostic;
}  // namespace

ExtensionIconVariants::ExtensionIconVariants() = default;

ExtensionIconVariants::ExtensionIconVariants(ExtensionIconVariants&& other) =
    default;

ExtensionIconVariants::~ExtensionIconVariants() = default;

void ExtensionIconVariants::Parse(const base::Value::List* list) {
  // Parse each icon variant in `icon_variants`.
  for (auto& entry : *list) {
    std::string issue;
    std::unique_ptr<ExtensionIconVariant> icon_variant =
        ExtensionIconVariant::Parse(entry, &issue);
    if (!icon_variant) {
      diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
          diagnostics::icon_variants::Feature::kIconVariants,
          diagnostics::icon_variants::Id::kEmptyIconVariant));
      continue;
    }

    // Move diagnostics for an icon_variant directly into icon_variants.
    auto& diagnostics = icon_variant->get_diagnostics();
    diagnostics_.insert(diagnostics_.end(), diagnostics.begin(),
                        diagnostics.end());
    diagnostics.clear();

    list_.emplace_back(std::move(*icon_variant));
  }

  // Add a warning for an empty list, but don't generate an error.
  if (list_.empty()) {
    diagnostics_.emplace_back(diagnostics::icon_variants::GetDiagnostic(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Id::kIconVariantsEmpty));
  }
}

bool ExtensionIconVariants::IsEmpty() const {
  return list_.size() == 0;
}

void ExtensionIconVariants::Add(
    std::unique_ptr<ExtensionIconVariant> icon_variant) {
  list_.emplace_back(std::move(*icon_variant));
}

}  //  namespace extensions
