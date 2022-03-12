// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_

#include "media/mojo/mojom/renderer_extensions.mojom-shared.h"
#include "media/renderers/win/media_foundation_renderer_extension.h"

template <>
struct mojo::EnumTraits<media::mojom::RenderingMode, media::RenderingMode> {
 public:
  static bool FromMojom(media::mojom::RenderingMode data,
                        media::RenderingMode* output) {
    switch (data) {
      case media::mojom::RenderingMode::DirectComposition:
        *output = media::RenderingMode::DirectComposition;
        break;
      case media::mojom::RenderingMode::FrameServer:
        *output = media::RenderingMode::FrameServer;
        break;
    }
    NOTREACHED();
    return false;
  }

  static media::mojom::RenderingMode ToMojom(media::RenderingMode data) {
    switch (data) {
      case media::RenderingMode::DirectComposition:
        return media::mojom::RenderingMode::DirectComposition;
        break;
      case media::RenderingMode::FrameServer:
        return media::mojom::RenderingMode::FrameServer;
        break;
    }
    NOTREACHED();
    return media::mojom::RenderingMode::DirectComposition;
  }
};

#endif  // MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_