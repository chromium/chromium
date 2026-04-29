// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/overlay_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::mojom::OverlayInfoDataView, media::OverlayInfo>::Read(
    media::mojom::OverlayInfoDataView input,
    media::OverlayInfo* output) {
  if (!input.ReadRoutingToken(&output->routing_token)) {
    return false;
  }

  output->is_fullscreen = input.is_fullscreen();
  output->is_persistent_video = input.is_persistent_video();

  return true;
}

}  // namespace mojo
