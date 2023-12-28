// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_

#include <list>
#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// An NaCl module included in the extension.
struct NaClModuleInfo {
  using List = std::list<NaClModuleInfo>;
  // Returns the NaCl modules for the extensions, or NULL if none exist.
  static const NaClModuleInfo::List* GetNaClModules(
      const Extension* extension);

  GURL url;
  std::string mime_type;
};

// Parses the "nacl_modules" manifest key.
class NaClModulesHandler : public ManifestHandler {
 public:
  NaClModulesHandler();
  ~NaClModulesHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_NACL_MODULES_HANDLER_H_
