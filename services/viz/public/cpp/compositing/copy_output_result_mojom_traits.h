// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_

#include <memory>
#include <optional>

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/compositing/bitmap_in_shared_memory_mojom_traits.h"
#include "services/viz/public/mojom/compositing/copy_output_result.mojom-shared.h"
#include "services/viz/public/mojom/compositing/texture_releaser.mojom.h"
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
struct EnumTraits<viz::mojom::CopyOutputResultDestination,
                  viz::CopyOutputResult::Destination> {
  static viz::mojom::CopyOutputResultDestination ToMojom(
      viz::CopyOutputResult::Destination destination);

  static bool FromMojom(viz::mojom::CopyOutputResultDestination input,
                        viz::CopyOutputResult::Destination* out);
};

template <>
struct StructTraits<viz::mojom::CopyOutputResultDataView,
                    std::unique_ptr<viz::CopyOutputResult>> {
  static viz::CopyOutputResult::Format format(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static viz::CopyOutputResult::Destination destination(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static const gfx::Rect& rect(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static std::optional<viz::CopyOutputResult::ScopedSkBitmap> bitmap(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static mojo::OptionalAsPointer<const gpu::Mailbox> mailbox(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static mojo::OptionalAsPointer<const gfx::ColorSpace> color_space(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static mojo::PendingRemote<viz::mojom::TextureReleaser> releaser(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static bool Read(viz::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<viz::CopyOutputResult>* out_p);

 private:
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_RESULT_MOJOM_TRAITS_H_
