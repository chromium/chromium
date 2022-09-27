// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

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
