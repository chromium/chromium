// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_DATA_H_

#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace content {
struct SocketPermissionRequest;
}

namespace extensions {
class SocketsManifestPermission;
}

namespace extensions {

// The parsed form of the "sockets" manifest entry.
class SocketsManifestData : public Extension::ManifestData {
 public:
  explicit SocketsManifestData(
      std::unique_ptr<SocketsManifestPermission> permission);
  ~SocketsManifestData() override;

  // Gets the SocketsManifestData for |extension|, or NULL if none was
  // specified.
  static SocketsManifestData* Get(const Extension* extension);

  static bool CheckRequest(const Extension* extension,
                           const content::SocketPermissionRequest& request);

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static std::unique_ptr<SocketsManifestData> FromValue(
      const base::Value& value,
      std::u16string* error);

  const SocketsManifestPermission* permission() const {
    return permission_.get();
  }

 private:
  std::unique_ptr<SocketsManifestPermission> permission_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SOCKETS_SOCKETS_MANIFEST_DATA_H_
