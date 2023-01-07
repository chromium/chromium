// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_

#include <string>
#include <vector>

#include "extensions/common/manifest_handler.h"

namespace extensions {
class Extension;
}

namespace extensions {

// Parses the "event_rules" manifest key.
class DeclarativeManifestHandler : public ManifestHandler {
 public:
  DeclarativeManifestHandler();

  DeclarativeManifestHandler(const DeclarativeManifestHandler&) = delete;
  DeclarativeManifestHandler& operator=(const DeclarativeManifestHandler&) =
      delete;

  ~DeclarativeManifestHandler() override;

  // ManifestHandler overrides.
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  // ManifestHandler overrides.
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_HANDLER_H_
