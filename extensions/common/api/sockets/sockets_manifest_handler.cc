// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_handler.h"

#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/api/sockets/sockets_manifest_permission.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

SocketsManifestHandler::SocketsManifestHandler() = default;

SocketsManifestHandler::~SocketsManifestHandler() = default;

bool SocketsManifestHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  const base::Value* sockets =
      extension->manifest()->FindPath(manifest_keys::kSockets);
  CHECK(sockets != nullptr);
  std::unique_ptr<SocketsManifestData> data =
      SocketsManifestData::FromValue(*sockets, error);
  if (!data)
    return false;

  extension->SetManifestData(manifest_keys::kSockets, std::move(data));
  return true;
}

ManifestPermission* SocketsManifestHandler::CreatePermission() {
  return new SocketsManifestPermission();
}

ManifestPermission* SocketsManifestHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  SocketsManifestData* data = SocketsManifestData::Get(extension);
  if (data)
    return data->permission()->Clone().release();
  return nullptr;
}

base::span<const char* const> SocketsManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kSockets};
  return kKeys;
}

}  // namespace extensions
