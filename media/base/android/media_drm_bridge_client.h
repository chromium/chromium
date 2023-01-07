// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CLIENT_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CLIENT_H_

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "media/base/media_export.h"

namespace media {

class MediaDrmBridgeClient;
class MediaDrmBridgeDelegate;

// Setter for MediaDrmBridgeClient. This should be called in all processes
// where we want to run media Android code. Also it should be called before any
// media playback could occur.
MEDIA_EXPORT void SetMediaDrmBridgeClient(MediaDrmBridgeClient* media_client);

#if defined(IS_MEDIA_IMPL)
// Getter for the client. Returns nullptr if no customized client is needed.
MediaDrmBridgeClient* GetMediaDrmBridgeClient();
#endif

using UUID = std::vector<uint8_t>;

// A client interface for embedders (e.g. content/browser or content/gpu) to
// provide customized additions to Android's media handling.
class MEDIA_EXPORT MediaDrmBridgeClient {
 public:
  typedef std::unordered_map<std::string, UUID> KeySystemUuidMap;

  MediaDrmBridgeClient();

  MediaDrmBridgeClient(const MediaDrmBridgeClient&) = delete;
  MediaDrmBridgeClient& operator=(const MediaDrmBridgeClient&) = delete;

  virtual ~MediaDrmBridgeClient();

  // Adds extra mappings from key-system name to Android UUID into |map|.
  virtual void AddKeySystemUUIDMappings(KeySystemUuidMap* map);

  // Returns a MediaDrmBridgeDelegate that corresponds to |scheme_uuid|.
  // MediaDrmBridgeClient retains ownership.
  virtual media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const UUID& scheme_uuid);

 private:
  friend class KeySystemManager;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CLIENT_H_
