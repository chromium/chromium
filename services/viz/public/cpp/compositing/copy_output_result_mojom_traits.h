// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/copy_output_result.mojom-shared.h"
#include "services/viz/public/mojom/compositing/texture_releaser.mojom.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::CopyOutputResultFormat,
                  viz::CopyOutputResult::Format> {
  static viz::mojom::CopyOutputResultFormat ToMojom(
      viz::CopyOutputResult::Format format);

  static bool FromMojom(viz::mojom::CopyOutputResultFormat input,
                        viz::CopyOutputResult::Format* out);
};

template <>
struct StructTraits<viz::mojom::CopyOutputResultDataView,
                    std::unique_ptr<viz::CopyOutputResult>> {
  static viz::CopyOutputResult::Format format(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static const gfx::Rect& rect(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static const SkBitmap& bitmap(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static base::Optional<gpu::Mailbox> mailbox(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static base::Optional<gpu::SyncToken> sync_token(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static base::Optional<gfx::ColorSpace> color_space(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static mojo::PendingRemote<viz::mojom::TextureReleaser> releaser(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static bool Read(viz::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<viz::CopyOutputResult>* out_p);

 private:
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_
