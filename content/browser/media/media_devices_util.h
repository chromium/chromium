// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "url/origin.h"

namespace content {

// Returns the ID of the user-default device ID via |callback|.
// If no such device ID can be found, |callback| receives an empty string.
CONTENT_EXPORT void GetDefaultMediaDeviceID(
    blink::MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(const std::string&)>& callback);

struct CONTENT_EXPORT MediaDeviceSaltAndOrigin {
  MediaDeviceSaltAndOrigin();
  MediaDeviceSaltAndOrigin(std::string device_id_salt,
                           std::string group_id_salt,
                           url::Origin origin);

  std::string device_id_salt;
  std::string group_id_salt;
  url::Origin origin;
};

// Returns the current media device ID salt and security origin for the given
// |render_process_id| and |render_frame_id|. These values are used to produce
// unique media-device IDs for each origin and renderer process. These values
// should not be cached since the user can explicitly change them at any time.
// This function must run on the UI thread.
CONTENT_EXPORT MediaDeviceSaltAndOrigin
GetMediaDeviceSaltAndOrigin(int render_process_id, int render_frame_id);

// Returns a translated version of |device_info| suitable for use in a renderer
// process.
// The |device_id| field is hashed using |device_id_salt| and |security_origin|.
// The |group_id| field is hashed using |group_id_salt| and |security_origin|.
// The |label| field is removed if |has_permission| is false.
blink::WebMediaDeviceInfo TranslateMediaDeviceInfo(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfo& device_info);

// Returns a translated version of |device_infos|, with each element translated
// using TranslateMediaDeviceInfo().
blink::WebMediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfoArray& device_infos);

// Type definition to make it easier to use mock alternatives to
// GetMediaDeviceSaltAndOrigin.
using MediaDeviceSaltAndOriginCallback =
    base::RepeatingCallback<MediaDeviceSaltAndOrigin(int, int)>;

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
