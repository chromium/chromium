// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_EXTENSION_ACTION_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_EXTENSION_ACTION_HANDLER_H_

#include <string>

#include "extensions/common/manifest_handler.h"

namespace extensions {

// Parses the "page_action" and "browser_action" manifest keys.
class ExtensionActionHandler : public ManifestHandler {
 public:
  ExtensionActionHandler();

  ExtensionActionHandler(const ExtensionActionHandler&) = delete;
  ExtensionActionHandler& operator=(const ExtensionActionHandler&) = delete;

  ~ExtensionActionHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  bool AlwaysParseForType(Manifest::Type type) const override;
  bool AlwaysValidateForType(Manifest::Type type) const override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_EXTENSION_ACTION_HANDLER_H_
