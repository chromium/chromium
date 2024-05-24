// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_

#include <memory>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/icons/extension_icon_variants.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct IconVariantsInfo : public Extension::ManifestData {
  IconVariantsInfo();
  ~IconVariantsInfo() override;

  // A map of mode to ExtensionIconSet, which represents the declared icons.
  std::unique_ptr<ExtensionIconVariants> icon_variants;

  // Returns the icon set for the given `extension`.
  static bool HasIconVariants(const Extension* extension);

  // Get IconVariants for the given `extension`, if they exist.
  static const IconVariantsInfo* GetIconVariants(const Extension* extension);
};

// Parses the "icon_variants" manifest key.
class IconVariantsHandler : public ManifestHandler {
 public:
  IconVariantsHandler();
  ~IconVariantsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICON_VARIANTS_HANDLER_H_
