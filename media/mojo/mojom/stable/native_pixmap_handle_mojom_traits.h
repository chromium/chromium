// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_STABLE_NATIVE_PIXMAP_HANDLE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STABLE_NATIVE_PIXMAP_HANDLE_MOJOM_TRAITS_H_

#include "media/mojo/mojom/stable/native_pixmap_handle.mojom.h"

namespace gfx {
struct NativePixmapHandle;
struct NativePixmapPlane;
}  // namespace gfx

namespace mojo {

template <>
struct StructTraits<media::stable::mojom::NativePixmapPlaneDataView,
                    gfx::NativePixmapPlane> {
  static uint32_t stride(const gfx::NativePixmapPlane& plane);

  static uint64_t offset(const gfx::NativePixmapPlane& plane);

  static uint64_t size(const gfx::NativePixmapPlane& plane);

  static mojo::PlatformHandle buffer_handle(gfx::NativePixmapPlane& plane);

  static bool Read(media::stable::mojom::NativePixmapPlaneDataView data,
                   gfx::NativePixmapPlane* out);
};

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
