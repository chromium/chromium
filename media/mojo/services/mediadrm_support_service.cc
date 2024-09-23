// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mediadrm_support_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "media/base/android/media_drm_bridge.h"

namespace media {

MediaDrmSupportService::MediaDrmSupportService(
    mojo::PendingReceiver<mojom::MediaDrmSupport> receiver)
    : receiver_(this, std::move(receiver)) {}

MediaDrmSupportService::~MediaDrmSupportService() = default;

void MediaDrmSupportService::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DCHECK(!key_system.empty());
  DVLOG(1) << __func__ << " key_system: " << key_system;

  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto result = mojom::MediaDrmSupportResult::New();
  result->key_system_supports_video_webm =
      MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/webm");
  result->key_system_supports_video_mp4 =
      MediaDrmBridge::IsKeySystemSupportedWithType(key_system, "video/mp4");
  std::move(callback).Run(std::move(result));
}

}  // namespace media
