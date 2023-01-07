// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_DELEGATE_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_DELEGATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "media/base/android/media_drm_bridge_client.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

namespace media {

// Allows embedders to modify the Android MediaDrm flow. Delegates are
// registered to a specific key system.
class MEDIA_EXPORT MediaDrmBridgeDelegate {
 public:
  MediaDrmBridgeDelegate();

  MediaDrmBridgeDelegate(const MediaDrmBridgeDelegate&) = delete;
  MediaDrmBridgeDelegate& operator=(const MediaDrmBridgeDelegate&) = delete;

  virtual ~MediaDrmBridgeDelegate();

  // Returns the UUID of the DRM scheme that this delegate applies to.
  virtual const UUID GetUUID() const = 0;

  // Invoked from CreateSession.
  // If |init_data_out| is filled, it replaces |init_data| to send to the
  // MediaDrm instance.
  // If |optional_parameters_out| is filled, it is expected to be an
  // even-length list of (key, value) pairs to send to the MediaDrm instance.
  // Returns false if the request should be rejected.
  virtual bool OnCreateSession(
      const EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::vector<uint8_t>* init_data_out,
      std::vector<std::string>* optional_parameters_out);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_DELEGATE_H_
