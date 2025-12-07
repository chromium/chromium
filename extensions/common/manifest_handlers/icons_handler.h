// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/icons/extension_icon_variant.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/icon_variants_handler.h"

class GURL;

namespace extensions {

struct IconsInfo : public Extension::ManifestData {
  // The icons for the extension.
  ExtensionIconSet icons;

  // Return the icon set for the given `extension`.
  static const ExtensionIconSet& GetIcons(const Extension* extension) {
    DCHECK(extension);
    return GetIcons(*extension, ExtensionIconVariant::ColorScheme::kLight);
  }
  static const ExtensionIconSet& GetIcons(
      const Extension& extension,
      std::optional<ExtensionIconVariant::ColorScheme> color_scheme);

  // Get an extension icon as a resource.
  static ExtensionResource GetIconResource(const Extension* extension,
                                           int size_in_px,
                                           ExtensionIconSet::Match match_type) {
    return GetIconResource(extension, size_in_px, match_type,
                           ExtensionIconVariant::ColorScheme::kLight);
  }
  static ExtensionResource GetIconResource(
      const Extension* extension,
      int size_in_px,
      ExtensionIconSet::Match match_type,
      ExtensionIconVariant::ColorScheme color_scheme);

  // Get an extension icon as a URL.
  static GURL GetIconURL(const Extension* extension,
                         int size_in_px,
                         ExtensionIconSet::Match match_type) {
    return GetIconURL(extension, size_in_px, match_type,
                      ExtensionIconVariant::ColorScheme::kLight);
  }
  static GURL GetIconURL(const Extension* extension,
                         int size_in_px,
                         ExtensionIconSet::Match match_type,
                         ExtensionIconVariant::ColorScheme color_scheme);
};

// Parses the "icons" manifest key.
class IconsHandler : public ManifestHandler {
 public:
  IconsHandler();
  ~IconsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension& extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_
