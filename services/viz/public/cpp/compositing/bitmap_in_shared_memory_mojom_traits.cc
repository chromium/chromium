// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/viz/public/cpp/compositing/bitmap_in_shared_memory_mojom_traits.h"

#include <cstdint>
#include <memory>

namespace {

void DeleteSharedMemoryMapping(void* not_used, void* context) {
  delete static_cast<base::WritableSharedMemoryMapping*>(context);
}

}  // namespace

namespace mojo {

// static
const SkImageInfo StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
                               viz::CopyOutputResult::ScopedSkBitmap>::
    image_info(const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap) {
  auto sk_bitmap = scoped_bitmap.bitmap();
  return sk_bitmap.info();
}

// static
uint64_t StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
                      viz::CopyOutputResult::ScopedSkBitmap>::
    row_bytes(const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap) {
  auto sk_bitmap = scoped_bitmap.bitmap();
  return sk_bitmap.info().minRowBytes();
}

// static
std::optional<base::WritableSharedMemoryRegion>
StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
             viz::CopyOutputResult::ScopedSkBitmap>::
    pixels(const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap) {
  auto sk_bitmap = scoped_bitmap.bitmap();
  if (!sk_bitmap.readyToDraw()) {
    return std::nullopt;
  }

  // The buffer for `sk_bitmap` could be larger than the minimum size required
  // to hold all the pixels. Minimize the shared memory allocation size here
  // since the pixel data is already being copied.
  size_t min_row_bytes = sk_bitmap.info().minRowBytes();
  size_t byte_size = sk_bitmap.info().computeByteSize(min_row_bytes);

  if (min_row_bytes == 0 || byte_size == 0) {
    return std::nullopt;
  }

  CHECK_GE(byte_size, sk_bitmap.height() * min_row_bytes);

  base::WritableSharedMemoryRegion region =
      base::WritableSharedMemoryRegion::Create(byte_size);
  {
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid()) {
      return std::nullopt;
    }

    auto* src_pixels = static_cast<const uint8_t*>(sk_bitmap.getPixels());
    size_t src_stride = sk_bitmap.rowBytes();
    auto* dst_pixels = static_cast<uint8_t*>(mapping.memory());

    // If source and destination stride are the same use a single copy
    // operation, otherwise do a row-by-row copy.
    if (src_stride == min_row_bytes) {
      memcpy(dst_pixels, src_pixels, byte_size);
    } else {
      for (int y = 0; y < sk_bitmap.height(); ++y) {
        memcpy(dst_pixels, src_pixels, min_row_bytes);
        src_pixels += src_stride;
        dst_pixels += min_row_bytes;
      }
    }
  }
  return region;
}

// static
bool StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap>::Read(
    viz::mojom::BitmapInSharedMemoryDataView data,
    SkBitmap* sk_bitmap) {
  SkImageInfo image_info;
  if (!data.ReadImageInfo(&image_info))
    return false;
  if (!image_info.validRowBytes(data.row_bytes()))
    return false;

  std::optional<base::WritableSharedMemoryRegion> region_opt;
  if (!data.ReadPixels(&region_opt))
    return false;

  *sk_bitmap = SkBitmap();
  if (!region_opt)
    return sk_bitmap->setInfo(image_info, data.row_bytes());

  auto mapping_ptr =
      std::make_unique<base::WritableSharedMemoryMapping>(region_opt->Map());
  if (!mapping_ptr->IsValid())
    return false;

  if (mapping_ptr->size() < image_info.computeByteSize(data.row_bytes())) {
    return false;
  }

  if (!sk_bitmap->installPixels(image_info, mapping_ptr->memory(),
                                data.row_bytes(), &DeleteSharedMemoryMapping,
                                mapping_ptr.get())) {
    return false;
  }
  mapping_ptr.release();
  return true;
}

}  // namespace mojo
