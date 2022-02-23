// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_STABLE_NATIVE_PIXMAP_HANDLE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STABLE_NATIVE_PIXMAP_HANDLE_MOJOM_TRAITS_H_

#include "media/mojo/mojom/stable/native_pixmap_handle.mojom.h"

namespace gfx {
struct NativePixmapHandle;
}  // namespace gfx

namespace mojo {

template <>
struct StructTraits<media::stable::mojom::NativePixmapHandleDataView,
                    gfx::NativePixmapHandle> {
  static std::vector<gfx::NativePixmapPlane>& planes(
      gfx::NativePixmapHandle& pixmap_handle);

  static uint64_t modifier(const gfx::NativePixmapHandle& pixmap_handle);

  static bool Read(media::stable::mojom::NativePixmapHandleDataView data,
                   gfx::NativePixmapHandle* out);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STABLE_NATIVE_PIXMAP_HANDLE_MOJOM_TRAITS_H_
