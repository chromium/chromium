// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
blink::mojom::FacingMode
EnumTraits<blink::mojom::FacingMode, media::VideoFacingMode>::ToMojom(
    media::VideoFacingMode facing_mode) {
  switch (facing_mode) {
    case media::MEDIA_VIDEO_FACING_NONE:
      return blink::mojom::FacingMode::NONE;
    case media::MEDIA_VIDEO_FACING_USER:
      return blink::mojom::FacingMode::USER;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return blink::mojom::FacingMode::ENVIRONMENT;
    default:
      break;
  }
  NOTREACHED();
  return blink::mojom::FacingMode::NONE;
}

// static
bool EnumTraits<blink::mojom::FacingMode, media::VideoFacingMode>::FromMojom(
    blink::mojom::FacingMode input,
    media::VideoFacingMode* out) {
  switch (input) {
    case blink::mojom::FacingMode::NONE:
      *out = media::MEDIA_VIDEO_FACING_NONE;
      return true;
    case blink::mojom::FacingMode::USER:
      *out = media::MEDIA_VIDEO_FACING_USER;
      return true;
    case blink::mojom::FacingMode::ENVIRONMENT:
      *out = media::MEDIA_VIDEO_FACING_ENVIRONMENT;
      return true;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<blink::mojom::MediaDeviceInfoDataView,
                  blink::WebMediaDeviceInfo>::
    Read(blink::mojom::MediaDeviceInfoDataView input,
         blink::WebMediaDeviceInfo* out) {
  if (!input.ReadDeviceId(&out->device_id))
    return false;
  if (!input.ReadLabel(&out->label))
    return false;
  if (!input.ReadGroupId(&out->group_id))
    return false;
  return true;
}

}  // namespace mojo
