// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "url/origin.h"

using blink::mojom::MediaDeviceType;

namespace content {

// Returns the ID of the user-default device ID via |callback|.
// If no such device ID can be found, |callback| receives an empty string.
CONTENT_EXPORT void GetDefaultMediaDeviceID(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(const std::string&)> callback);

struct CONTENT_EXPORT MediaDeviceSaltAndOrigin {
  MediaDeviceSaltAndOrigin();
  MediaDeviceSaltAndOrigin(std::string device_id_salt,
                           std::string group_id_salt,
                           url::Origin origin,
                           bool has_focus,
                           bool is_background);
  MediaDeviceSaltAndOrigin(const MediaDeviceSaltAndOrigin& other);
  ~MediaDeviceSaltAndOrigin();

  std::string device_id_salt;
  std::string group_id_salt;
  // Last committed origin of the frame making a media device request.
  url::Origin origin;
  // ukm::SourceId of the main frame making the media device request.
  absl::optional<ukm::SourceId> ukm_source_id;
  bool has_focus;
  bool is_background;
};

// Returns the current media device ID salt and security origin for the given
// |render_process_id| and |render_frame_id|. These values are used to produce
// unique media-device IDs for each origin and renderer process. These values
// should not be cached since the user can explicitly change them at any time.
// This function must run on the UI thread.
using MediaDeviceSaltAndOriginCallback =
    base::OnceCallback<void(const MediaDeviceSaltAndOrigin&)>;
CONTENT_EXPORT void GetMediaDeviceSaltAndOrigin(
    GlobalRenderFrameHostId render_frame_host_id,
    MediaDeviceSaltAndOriginCallback callback);

// Type definition to make it easier to use mock alternatives to
// GetMediaDeviceSaltAndOrigin.
using GetMediaDeviceSaltAndOriginCallback =
    base::RepeatingCallback<void(GlobalRenderFrameHostId,
                                 MediaDeviceSaltAndOriginCallback)>;

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

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
