// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/native_pixmap_handle_mojom_traits.h"

#include "ui/gfx/native_pixmap_handle.h"

// This file contains a variety of conservative compile-time assertions that
// help us detect changes that may break the backward compatibility requirement
// of the StableVideoDecoder API. Specifically, we have static_asserts() that
// ensure the type of the gfx struct member is *exactly* the same as the
// corresponding mojo struct member. If this changes, we must be careful to
// validate ranges and avoid implicit conversions.
//
// If you need to make any changes to this file, please consult with
// chromeos-gfx-video@google.com first.

namespace mojo {

// static
uint32_t StructTraits<
    media::stable::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::stride(const gfx::NativePixmapPlane& plane) {
  static_assert(
      std::is_same<decltype(::gfx::NativePixmapPlane::stride),
                   decltype(
                       media::stable::mojom::NativePixmapPlane::stride)>::value,
      "Unexpected type for gfx::NativePixmapPlane::stride. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return plane.stride;
}

// static
uint64_t StructTraits<
    media::stable::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::offset(const gfx::NativePixmapPlane& plane) {
  static_assert(
      std::is_same<decltype(::gfx::NativePixmapPlane::offset),
                   decltype(
                       media::stable::mojom::NativePixmapPlane::offset)>::value,
      "Unexpected type for gfx::NativePixmapPlane::offset. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return plane.offset;
}

// static
uint64_t StructTraits<
    media::stable::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::size(const gfx::NativePixmapPlane& plane) {
  static_assert(
      std::is_same<decltype(::gfx::NativePixmapPlane::size),
                   decltype(
                       media::stable::mojom::NativePixmapPlane::size)>::value,
      "Unexpected type for gfx::NativePixmapPlane::size. If you need to change "
      "this assertion, please contact chromeos-gfx-video@google.com.");

  return plane.size;
}

// static
mojo::PlatformHandle StructTraits<
    media::stable::mojom::NativePixmapPlaneDataView,
    gfx::NativePixmapPlane>::buffer_handle(gfx::NativePixmapPlane& plane) {
  static_assert(
      std::is_same<decltype(::gfx::NativePixmapPlane::fd),
                   base::ScopedFD>::value,
      "Unexpected type for gfx::NativePixmapPlane::fd. If you need to change "
      "this assertion, please contact chromeos-gfx-video@google.com.");
  CHECK(plane.fd.is_valid());
  return mojo::PlatformHandle(std::move(plane.fd));
}

// static
bool StructTraits<media::stable::mojom::NativePixmapPlaneDataView,
                  gfx::NativePixmapPlane>::
    Read(media::stable::mojom::NativePixmapPlaneDataView data,
         gfx::NativePixmapPlane* out) {
  out->stride = data.stride();
  out->offset = data.offset();
  out->size = data.size();

  mojo::PlatformHandle handle = data.TakeBufferHandle();
  if (!handle.is_fd() || !handle.is_valid_fd())
    return false;
  out->fd = handle.TakeFD();
  return true;
}

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
