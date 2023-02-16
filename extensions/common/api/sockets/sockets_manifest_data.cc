// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_data.h"

#include <memory>
#include <utility>

#include "extensions/common/api/sockets/sockets_manifest_permission.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

SocketsManifestData::SocketsManifestData(
    std::unique_ptr<SocketsManifestPermission> permission)
    : permission_(std::move(permission)) {
  DCHECK(permission_);
}

SocketsManifestData::~SocketsManifestData() = default;

// static
SocketsManifestData* SocketsManifestData::Get(const Extension* extension) {
  return static_cast<SocketsManifestData*>(
      extension->GetManifestData(manifest_keys::kSockets));
}

// static
bool SocketsManifestData::CheckRequest(
    const Extension* extension,
    const content::SocketPermissionRequest& request) {
  const SocketsManifestData* data = SocketsManifestData::Get(extension);
  if (data)
    return data->permission()->CheckRequest(extension, request);

  return false;
}

// static
std::unique_ptr<SocketsManifestData> SocketsManifestData::FromValue(
    const base::Value& value,
    std::u16string* error) {
  std::unique_ptr<SocketsManifestPermission> permission =
      SocketsManifestPermission::FromValue(value, error);
  if (!permission)
    return nullptr;

  return std::make_unique<SocketsManifestData>(std::move(permission));
}

}  // namespace extensions
