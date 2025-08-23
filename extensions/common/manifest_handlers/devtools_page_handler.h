// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_DEVTOOLS_PAGE_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_DEVTOOLS_PAGE_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

namespace chrome_manifest_urls {
const GURL& GetDevToolsPage(const Extension* extension);
}

// Parses the "devtools_page" manifest key.
class DevToolsPageHandler : public ManifestHandler {
 public:
  DevToolsPageHandler();

  DevToolsPageHandler(const DevToolsPageHandler&) = delete;
  DevToolsPageHandler& operator=(const DevToolsPageHandler&) = delete;

  ~DevToolsPageHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension& extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_DEVTOOLS_PAGE_HANDLER_H_
