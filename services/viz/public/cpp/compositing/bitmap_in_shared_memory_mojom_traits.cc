// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/bitmap_in_shared_memory_mojom_traits.h"

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
  return sk_bitmap.rowBytes();
}

// static
absl::optional<base::WritableSharedMemoryRegion>
StructTraits<viz::mojom::BitmapInSharedMemoryDataView,
             viz::CopyOutputResult::ScopedSkBitmap>::
    pixels(const viz::CopyOutputResult::ScopedSkBitmap& scoped_bitmap) {
  auto sk_bitmap = scoped_bitmap.bitmap();
  if (!sk_bitmap.readyToDraw())
    return absl::nullopt;

  size_t byte_size = sk_bitmap.computeByteSize();
  base::WritableSharedMemoryRegion region =
      base::WritableSharedMemoryRegion::Create(byte_size);
  {
    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid())
      return absl::nullopt;
    memcpy(mapping.memory(), sk_bitmap.getPixels(), byte_size);
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

  absl::optional<base::WritableSharedMemoryRegion> region_opt;
  if (!data.ReadPixels(&region_opt))
    return false;

  *sk_bitmap = SkBitmap();
  if (!region_opt)
    return sk_bitmap->setInfo(image_info, data.row_bytes());

  auto mapping_ptr =
      std::make_unique<base::WritableSharedMemoryMapping>(region_opt->Map());
  if (!mapping_ptr->IsValid())
    return false;

  if (!sk_bitmap->installPixels(image_info, mapping_ptr->memory(),
                                data.row_bytes(), &DeleteSharedMemoryMapping,
                                mapping_ptr.get())) {
    return false;
  }
  mapping_ptr.release();
  return true;
}

}  // namespace mojo
