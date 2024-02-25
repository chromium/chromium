// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/common/features.h"
#include "content/public/browser/global_routing_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

namespace content {

using DeviceIdCallback = base::OnceCallback<void(const std::string&)>;

class CONTENT_EXPORT MediaDeviceSaltAndOrigin {
 public:
  MediaDeviceSaltAndOrigin(
      std::string device_id_salt,
      url::Origin origin,
      std::string group_id_salt = std::string(),
      bool has_focus = false,
      bool is_background = false,
      std::optional<ukm::SourceId> ukm_source_id = std::nullopt);

  MediaDeviceSaltAndOrigin(const MediaDeviceSaltAndOrigin& other);
  MediaDeviceSaltAndOrigin& operator=(const MediaDeviceSaltAndOrigin& other);

  MediaDeviceSaltAndOrigin(MediaDeviceSaltAndOrigin&& other);
  MediaDeviceSaltAndOrigin& operator=(MediaDeviceSaltAndOrigin&& other);

  ~MediaDeviceSaltAndOrigin();

  static MediaDeviceSaltAndOrigin Empty();

  const std::string& device_id_salt() const { return device_id_salt_; }
  const std::string& group_id_salt() const { return group_id_salt_; }
  const url::Origin& origin() const { return origin_; }
  const std::optional<ukm::SourceId>& ukm_source_id() const {
    return ukm_source_id_;
  }
  bool has_focus() const { return has_focus_; }
  bool is_background() const { return is_background_; }

  void set_device_id_salt(std::string salt) {
    device_id_salt_ = std::move(salt);
  }
  void set_group_id_salt(std::string salt) { group_id_salt_ = std::move(salt); }
  void set_ukm_source_id(std::optional<ukm::SourceId> ukm_source_id) {
    ukm_source_id_ = std::move(ukm_source_id);
  }
  void set_origin(url::Origin origin) { origin_ = std::move(origin); }
  void set_has_focus(bool has_focus) { has_focus_ = has_focus; }
  void set_is_background(bool is_background) { is_background_ = is_background; }

 private:
  std::string device_id_salt_;
  std::string group_id_salt_;
  // Last committed origin of the frame making a media device request.
  url::Origin origin_;
  // ukm::SourceId of the main frame making the media device request.
  std::optional<ukm::SourceId> ukm_source_id_;
  bool has_focus_;
  bool is_background_;
};

// Returns the current media device ID salt and security origin for the given
// `render_frame_host_id`. The returned value can be used to produce unique
// media-device IDs for the origin associated with `render_frame_host_id` and
// it should not be cached since the user can explicitly change it at any time.
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

// Returns a translated (HMAC) version of (raw) `device_info` suitable for use
// in a renderer process.
// The `device_id` field is hashed using the `device_id_salt` and `origin` in
// `salt_and_origin`.
// The `group_id` field is hashed using the `group_id_salt` and `origin` in
// `salt_and_origin`.
// If `has_permission` is false all fields in the returned value are empty.
CONTENT_EXPORT blink::WebMediaDeviceInfo TranslateMediaDeviceInfo(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfo& device_info);

// Returns a translated (HMAC) version of raw `device_infos`, with each element
// translated using TranslateMediaDeviceInfo(). If `has_permission` is false,
// the output will contain at most one element per device type with empty values
// for all fields, as per
// https://w3c.github.io/mediacapture-main/#access-control-model
CONTENT_EXPORT blink::WebMediaDeviceInfoArray TranslateMediaDeviceInfoArray(
    bool has_permission,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const blink::WebMediaDeviceInfoArray& device_infos);

// Creates a random salt that can be used to generate media device IDs that can
// be sent to the renderer process.
CONTENT_EXPORT std::string CreateRandomMediaDeviceIDSalt();

// Returns via `hmac_device_id_callback` an HMAC-translated version of
//`raw_device_id` suitable for use by the given `render_frame_host_id`.
CONTENT_EXPORT void GetHMACFromRawDeviceId(
    GlobalRenderFrameHostId render_frame_host_id,
    const std::string& raw_device_id,
    DeviceIdCallback hmac_device_id_callback);

using OptionalDeviceIdCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;
// Returns via `raw_device_id_callback` the raw version of `hmac_device_id`.
// The salt for the given `render_frame_host_id` is used to translate
// `hmac_device_id`. If no device of type `media_device_type` which corresponds
// to the given `hmac_device_id` is found in the system, this function returns
// `std::nullopt`.
CONTENT_EXPORT void GetRawDeviceIdFromHMAC(
    GlobalRenderFrameHostId render_frame_host_id,
    const std::string& hmac_device_id,
    blink::mojom::MediaDeviceType media_device_type,
    OptionalDeviceIdCallback raw_device_id_callback);

// Generates an HMAC device ID for `raw_device_id` using the given
// `salt_and_origin`. If `use_group_salt` is true,
// `salt_and_origin.group_id_salt()` will be used as salt. Otherwise,
// `salt_and_origin.device_id_salt()` is used as salt.
CONTENT_EXPORT std::string GetHMACForRawMediaDeviceID(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& raw_device_id,
    bool use_group_salt = false);

// Checks if `hmac_device_id` is an HMAC of |raw_device_id| for the given
// `salt_and_origin`.
CONTENT_EXPORT bool DoesRawMediaDeviceIDMatchHMAC(
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const std::string& hmac_device_id,
    const std::string& raw_unique_id);

// Returns the raw device ID for `hmac_device_id` for the given
// `salt_and_origin`. `stream_type` must be
// blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE or
// blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE. The result will
// be returned via `callback` on the given `task_runner`.
// Must be called on the IO thread.
CONTENT_EXPORT void GetRawDeviceIDForMediaStreamHMAC(
    blink::mojom::MediaStreamType stream_type,
    MediaDeviceSaltAndOrigin salt_and_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OptionalDeviceIdCallback callback);

// Returns the raw device ID of type `device_type` for `hmac_device_id` for the
// given `salt_and_origin`. The result will be returned via `callback` on the
// given `task_runner`.
// Must be called on the IO thread.
CONTENT_EXPORT void GetRawDeviceIDForMediaDeviceHMAC(
    blink::mojom::MediaDeviceType device_type,
    MediaDeviceSaltAndOrigin salt_and_origin,
    std::string hmac_device_id,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OptionalDeviceIdCallback callback);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_DEVICES_UTIL_H_
