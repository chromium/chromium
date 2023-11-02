// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_PUBLIC_MOJOM_BITMAP_SKBITMAP_MOJOM_TRAITS_H_
#define SKIA_PUBLIC_MOJOM_BITMAP_SKBITMAP_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "skia/public/mojom/bitmap.mojom-shared.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

// Struct traits to convert between SkBitmap and mojom types.

template <>
struct COMPONENT_EXPORT(SKIA_SHARED_TRAITS)
    StructTraits<skia::mojom::BitmapN32DataView, SkBitmap> {
  static bool IsNull(const SkBitmap& b) { return b.isNull(); }
  static void SetToNull(SkBitmap* b) { b->reset(); }

  static const SkImageInfo& image_info(const SkBitmap& b) { return b.info(); }
  static mojo_base::BigBufferView pixel_data(const SkBitmap& b);

  static bool Read(skia::mojom::BitmapN32DataView data, SkBitmap* b);
};

template <>
struct COMPONENT_EXPORT(SKIA_SHARED_TRAITS)
    StructTraits<skia::mojom::BitmapWithArbitraryBppDataView, SkBitmap> {
  static bool IsNull(const SkBitmap& b) { return b.isNull(); }
  static void SetToNull(SkBitmap* b) { b->reset(); }

  static const SkImageInfo& image_info(const SkBitmap& b) { return b.info(); }
  static uint64_t UNUSED_row_bytes(const SkBitmap& b) { return 0; }
  static mojo_base::BigBufferView pixel_data(const SkBitmap& b);

  static bool Read(skia::mojom::BitmapWithArbitraryBppDataView data,
                   SkBitmap* b);
};

template <>
struct COMPONENT_EXPORT(SKIA_SHARED_TRAITS)
    StructTraits<skia::mojom::BitmapMappedFromTrustedProcessDataView,
                 SkBitmap> {
  static bool IsNull(const SkBitmap& b) { return b.isNull(); }
  static void SetToNull(SkBitmap* b) { b->reset(); }

  static const SkImageInfo& image_info(const SkBitmap& b) { return b.info(); }
  static uint64_t UNUSED_row_bytes(const SkBitmap& b) { return 0; }
  static mojo_base::BigBufferView pixel_data(const SkBitmap& b);

  static bool Read(skia::mojom::BitmapMappedFromTrustedProcessDataView data,
                   SkBitmap* b);
};

template <>
struct COMPONENT_EXPORT(SKIA_SHARED_TRAITS)
    StructTraits<skia::mojom::InlineBitmapDataView, SkBitmap> {
  static bool IsNull(const SkBitmap& b) { return b.isNull(); }
  static void SetToNull(SkBitmap* b) { b->reset(); }

  static const SkImageInfo& image_info(const SkBitmap& b) { return b.info(); }
  static base::span<const uint8_t> pixel_data(const SkBitmap& b);

  static bool Read(skia::mojom::InlineBitmapDataView data, SkBitmap* b);
};

}  // namespace mojo

#endif  // SKIA_PUBLIC_MOJOM_BITMAP_SKBITMAP_MOJOM_TRAITS_H_
