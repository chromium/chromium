// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::MediaDeviceType, blink::MediaDeviceType> {
  static blink::mojom::MediaDeviceType ToMojom(blink::MediaDeviceType type);

  static bool FromMojom(blink::mojom::MediaDeviceType input,
                        blink::MediaDeviceType* out);
};

template <>
struct BLINK_COMMON_EXPORT
    EnumTraits<blink::mojom::FacingMode, media::VideoFacingMode> {
  static blink::mojom::FacingMode ToMojom(media::VideoFacingMode facing_mode);

  static bool FromMojom(blink::mojom::FacingMode input,
                        media::VideoFacingMode* out);
};

template <>
struct BLINK_COMMON_EXPORT StructTraits<blink::mojom::MediaDeviceInfoDataView,
                                        blink::WebMediaDeviceInfo> {
  static const std::string& device_id(const blink::WebMediaDeviceInfo& info) {
    return info.device_id;
  }

  static const std::string& label(const blink::WebMediaDeviceInfo& info) {
    return info.label;
  }

  static const std::string& group_id(const blink::WebMediaDeviceInfo& info) {
    return info.group_id;
  }

  static bool Read(blink::mojom::MediaDeviceInfoDataView input,
                   blink::WebMediaDeviceInfo* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_MOJOM_TRAITS_H_
