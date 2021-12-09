// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/native_pixmap_handle_mojom_traits.h"

#include "ui/gfx/native_pixmap_handle.h"

// This file contains a variety of conservative compile-time assertions that
// help us detect changes that may break the backward compatibility requirement
// of the StableVideoDecoder API. Specifically, we have static_asserts() that
// ensure the type of the media struct member is *exactly* the same as the
// corresponding mojo struct member. If this changes, we must be careful to
// validate ranges and avoid implicit conversions.
//
// If you need to make any changes to this file, please consult with
// chromeos-gfx-video@google.com first.

namespace mojo {

// static
std::vector<gfx::NativePixmapPlane>& StructTraits<
    media::stable::mojom::NativePixmapHandleDataView,
    gfx::NativePixmapHandle>::planes(gfx::NativePixmapHandle& pixmap_handle) {
  static_assert(
      std::is_same<
          decltype(::gfx::NativePixmapHandle::planes),
          decltype(media::stable::mojom::NativePixmapHandle::planes)>::value,
      "Unexpected type for gfx::NativePixmapHandle::planes. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return pixmap_handle.planes;
}

// static
uint64_t
StructTraits<media::stable::mojom::NativePixmapHandleDataView,
             gfx::NativePixmapHandle>::modifier(const gfx::NativePixmapHandle&
                                                    pixmap_handle) {
  static_assert(
      std::is_same<
          decltype(::gfx::NativePixmapHandle::modifier),
          decltype(media::stable::mojom::NativePixmapHandle::modifier)>::value,
      "Unexpected type for gfx::NativePixmapHandle::modifier. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return pixmap_handle.modifier;
}

// static
bool StructTraits<media::stable::mojom::NativePixmapHandleDataView,
                  gfx::NativePixmapHandle>::
    Read(media::stable::mojom::NativePixmapHandleDataView data,
         gfx::NativePixmapHandle* out) {
  out->modifier = data.modifier();
  return data.ReadPlanes(&out->planes);
}

}  // namespace mojo
