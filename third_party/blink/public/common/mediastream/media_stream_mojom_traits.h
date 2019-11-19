// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::MediaStreamDeviceDataView,
                                        blink::MediaStreamDevice> {
  static const blink::mojom::MediaStreamType& type(
      const blink::MediaStreamDevice& device) {
    return device.type;
  }

  static const std::string& id(const blink::MediaStreamDevice& device) {
    return device.id;
  }

  static const media::VideoFacingMode& video_facing(
      const blink::MediaStreamDevice& device) {
    return device.video_facing;
  }

  static const base::Optional<std::string>& group_id(
      const blink::MediaStreamDevice& device) {
    return device.group_id;
  }

  static const base::Optional<std::string>& matched_output_device_id(
      const blink::MediaStreamDevice& device) {
    return device.matched_output_device_id;
  }

  static const std::string& name(const blink::MediaStreamDevice& device) {
    return device.name;
  }

  static const media::AudioParameters& input(
      const blink::MediaStreamDevice& device) {
    return device.input;
  }

  static const base::Optional<base::UnguessableToken>& session_id(
      const blink::MediaStreamDevice& device) {
    return device.serializable_session_id();
  }

  static const base::Optional<media::mojom::DisplayMediaInformationPtr>&
  display_media_info(const blink::MediaStreamDevice& device) {
    return device.display_media_info;
  }

  static bool Read(blink::mojom::MediaStreamDeviceDataView input,
                   blink::MediaStreamDevice* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::TrackControlsDataView, blink::TrackControls> {
  static bool requested(const blink::TrackControls& controls) {
    return controls.requested;
  }

  static const blink::mojom::MediaStreamType& stream_type(
      const blink::TrackControls& controls) {
    return controls.stream_type;
  }

  static const std::string& device_id(const blink::TrackControls& controls) {
    return controls.device_id;
  }

  static bool Read(blink::mojom::TrackControlsDataView input,
                   blink::TrackControls* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::StreamControlsDataView, blink::StreamControls> {
  static const blink::TrackControls& audio(
      const blink::StreamControls& controls) {
    return controls.audio;
  }

  static const blink::TrackControls& video(
      const blink::StreamControls& controls) {
    return controls.video;
  }

  static bool hotword_enabled(const blink::StreamControls& controls) {
    return controls.hotword_enabled;
  }

  static bool disable_local_echo(const blink::StreamControls& controls) {
    return controls.disable_local_echo;
  }

  static bool Read(blink::mojom::StreamControlsDataView input,
                   blink::StreamControls* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_STREAM_MOJOM_TRAITS_H_
