// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_client.h"

#include "base/check.h"

namespace media {

static MediaDrmBridgeClient* g_media_drm_bridge_client = nullptr;

void SetMediaDrmBridgeClient(MediaDrmBridgeClient* media_client) {
  DCHECK(!g_media_drm_bridge_client);
  g_media_drm_bridge_client = media_client;
}

MediaDrmBridgeClient* GetMediaDrmBridgeClient() {
  return g_media_drm_bridge_client;
}

MediaDrmBridgeClient::MediaDrmBridgeClient() {}

MediaDrmBridgeClient::~MediaDrmBridgeClient() {}

void MediaDrmBridgeClient::AddKeySystemUUIDMappings(KeySystemUuidMap* map) {}

media::MediaDrmBridgeDelegate* MediaDrmBridgeClient::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  return nullptr;
}

}  // namespace media
