// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_

#include <memory>
#include <string>

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_variant.h"
#include "extensions/common/icons/extension_icon_variants.h"
#include "extensions/common/manifest_handler.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

struct IconVariantsInfo : public Extension::ManifestData {
  IconVariantsInfo();
  ~IconVariantsInfo() override;

  // Returns whether `icon_variants` are defined for the given `extension`.
  static bool HasIconVariants(const Extension* extension);

  // Get IconVariants for the given `extension`, if they exist.
  static const IconVariantsInfo* GetIconVariants(const Extension& extension);

  // Available e.g. when the extension feature enabled.
  static bool SupportsIconVariants(const Extension& extension);

  // Retrieve a matching ExtensionIconSet.
  const ExtensionIconSet& Get() const {
    return Get(ExtensionIconVariant::ColorScheme::kLight);
  }
  const ExtensionIconSet& Get(
      std::optional<ExtensionIconVariant::ColorScheme> color_scheme) const;

  // Data structure for `icon_variants`, based on icon_variants.idl.
  std::optional<ExtensionIconVariants> icon_variants;

  // Populate member variable extension sets from `icon_variants`.
  void InitializeIconSets();

 private:
  // Allow for easy access to the theme-related sets
  ExtensionIconSet dark_;
  ExtensionIconSet light_;
};

// Parses the "icon_variants" manifest key.
class IconVariantsHandler : public ManifestHandler {
 public:
  IconVariantsHandler();
  ~IconVariantsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension& extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_
