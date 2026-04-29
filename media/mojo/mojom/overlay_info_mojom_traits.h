// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_OVERLAY_INFO_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_OVERLAY_INFO_MOJOM_TRAITS_H_

#include <optional>

#include "base/unguessable_token.h"
#include "media/base/overlay_info.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::OverlayInfoDataView, media::OverlayInfo> {
  static const std::optional<base::UnguessableToken>& routing_token(
      const media::OverlayInfo& input) {
    return input.routing_token;
  }

  static bool is_fullscreen(const media::OverlayInfo& input) {
    return input.is_fullscreen;
  }

  static bool is_persistent_video(const media::OverlayInfo& input) {
    return input.is_persistent_video;
  }

  static bool Read(media::mojom::OverlayInfoDataView input,
                   media::OverlayInfo* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_OVERLAY_INFO_MOJOM_TRAITS_H_
