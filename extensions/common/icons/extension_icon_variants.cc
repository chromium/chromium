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

ExtensionIconVariants::~ExtensionIconVariants() = default;

ExtensionIconVariants::ExtensionIconVariants(ExtensionIconVariants&& other) =
    default;

// TODO(crbug.com/41419485): Include `warning` in addition to `error`.
bool ExtensionIconVariants::Parse(const base::Value::List* list,
                                  std::vector<Diagnostic>& diagnostics) {
  // Parse each icon variant in `icon_variants`.
  for (auto& entry : *list) {
    std::string issue;
    auto icon_variant = ExtensionIconVariant::Parse(entry, &issue);
    if (!icon_variant.has_value()) {
      diagnostics.emplace_back(diagnostics::icon_variants::GetDiagnosticForID(
          diagnostics::icon_variants::Feature::kIconVariants,
          diagnostics::icon_variants::Code::kEmptyIconVariant));
      continue;
    }
    list_.emplace_back(std::move(icon_variant.value()));
  }

  // Add a warning for an empty list, but don't generate an error.
  if (list_.empty()) {
    diagnostics.emplace_back(diagnostics::icon_variants::GetDiagnosticForID(
        diagnostics::icon_variants::Feature::kIconVariants,
        diagnostics::icon_variants::Code::kIconVariantsEmpty));
  }

  return true;
}

bool ExtensionIconVariants::IsEmpty() const {
  return list_.size() == 0;
}

}  //  namespace extensions
