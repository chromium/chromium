// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_HANDLER_H_

#include <string>
#include <vector>

#include "extensions/common/manifest_handler.h"

namespace extensions {
class Extension;
class ManifestPermission;
}

namespace extensions {

// Parses the "sockets" manifest key.
class SocketsManifestHandler : public ManifestHandler {
 public:
  SocketsManifestHandler();

  SocketsManifestHandler(const SocketsManifestHandler&) = delete;
  SocketsManifestHandler& operator=(const SocketsManifestHandler&) = delete;

  ~SocketsManifestHandler() override;

  // ManifestHandler overrides.
  bool Parse(Extension* extension, std::u16string* error) override;
  ManifestPermission* CreatePermission() override;
  ManifestPermission* CreateInitialRequiredPermission(
      const Extension* extension) override;

 private:
  // ManifestHandler overrides.
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_HANDLER_H_
