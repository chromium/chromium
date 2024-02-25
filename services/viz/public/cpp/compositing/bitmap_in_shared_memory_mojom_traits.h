// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_

#include <optional>

#include "base/memory/writable_shared_memory_region.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "services/viz/public/mojom/compositing/bitmap_in_shared_memory.mojom-shared.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
                    viz::CopyOutputResult::ScopedSkBitmap> {
  static const SkImageInfo image_info(
      const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap);

  static uint64_t row_bytes(
      const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap);

  static std::optional<base::WritableSharedMemoryRegion> pixels(
      const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap);

  static bool Read(viz::mojom::BitmapInSharedMemoryDataView data,
                   SkBitmap* sk_bitmap);
};

template <>
struct StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap> {
  static bool Read(viz::mojom::BitmapInSharedMemoryDataView data,
                   SkBitmap* sk_bitmap);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_
