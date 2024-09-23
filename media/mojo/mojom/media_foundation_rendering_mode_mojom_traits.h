// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_

#include "media/mojo/mojom/renderer_extensions.mojom-shared.h"
#include "media/renderers/win/media_foundation_rendering_mode.h"

template <>
struct mojo::EnumTraits<media::mojom::MediaFoundationRenderingMode,
                        media::MediaFoundationRenderingMode> {
 public:
  static bool FromMojom(media::mojom::MediaFoundationRenderingMode data,
                        media::MediaFoundationRenderingMode* output) {
    switch (data) {
      case media::mojom::MediaFoundationRenderingMode::DirectComposition:
        *output = media::MediaFoundationRenderingMode::DirectComposition;
        return true;
      case media::mojom::MediaFoundationRenderingMode::FrameServer:
        *output = media::MediaFoundationRenderingMode::FrameServer;
        return true;
    }
    NOTREACHED();
  }

  static media::mojom::MediaFoundationRenderingMode ToMojom(
      media::MediaFoundationRenderingMode data) {
    switch (data) {
      case media::MediaFoundationRenderingMode::DirectComposition:
        return media::mojom::MediaFoundationRenderingMode::DirectComposition;
      case media::MediaFoundationRenderingMode::FrameServer:
        return media::mojom::MediaFoundationRenderingMode::FrameServer;
        break;
    }
    NOTREACHED();
  }
};

#endif  // MEDIA_MOJO_MOJOM_MEDIA_FOUNDATION_RENDERING_MODE_MOJOM_TRAITS_H_
