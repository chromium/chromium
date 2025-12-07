// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BLIT_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BLIT_REQUEST_MOJOM_TRAITS_H_

#include "components/viz/common/frame_sinks/blit_request.h"
#include "services/viz/public/mojom/compositing/blit_request.mojom-shared.h"

namespace gpu {
struct ExportedSharedImage;
}

namespace mojo {

template <>
struct EnumTraits<viz::mojom::LetterboxingBehavior, viz::LetterboxingBehavior> {
  static viz::mojom::LetterboxingBehavior ToMojom(
      viz::LetterboxingBehavior behavior);
  static bool FromMojom(viz::mojom::LetterboxingBehavior input,
                        viz::LetterboxingBehavior* out);
};

template <>
struct StructTraits<viz::mojom::BlitRequestDataView, viz::BlitRequest> {
  static const gfx::Point& destination_region_offset(
      const viz::BlitRequest& blit_request) {
    return blit_request.destination_region_offset();
  }

  static viz::LetterboxingBehavior letterboxing_behavior(
      const viz::BlitRequest& blit_request) {
    return blit_request.letterboxing_behavior();
  }

  static gpu::ExportedSharedImage shared_image(
      const viz::BlitRequest& blit_request);

  static const gpu::SyncToken& sync_token(
      const viz::BlitRequest& blit_request) {
    return blit_request.sync_token();
  }

  static bool populates_gpu_memory_buffer(
      const viz::BlitRequest& blit_request) {
    return blit_request.populates_gpu_memory_buffer();
  }

  static bool Read(viz::mojom::BlitRequestDataView data, viz::BlitRequest* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BLIT_REQUEST_MOJOM_TRAITS_H_
