// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handler.h"

class GURL;

namespace extensions {

struct IconsInfo : public Extension::ManifestData {
  // The icons for the extension.
  ExtensionIconSet icons;

  // Return the icon set for the given |extension|.
  static const ExtensionIconSet& GetIcons(const Extension* extension);

  // Get an extension icon as a resource or URL.
  static ExtensionResource GetIconResource(const Extension* extension,
                                           int size_in_px,
                                           ExtensionIconSet::Match match_type);
  static GURL GetIconURL(const Extension* extension,
                         int size_in_px,
                         ExtensionIconSet::Match match_type);
};

// Parses the "icons" manifest key.
class IconsHandler : public ManifestHandler {
 public:
  IconsHandler();
  ~IconsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_ICONS_HANDLER_H_
