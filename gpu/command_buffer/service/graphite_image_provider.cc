// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "third_party/skia/include/gpu/graphite/Image.h"

namespace gpu {

GraphiteImageProvider::GraphiteImageProvider(size_t max_cache_bytes)
    : preferred_max_cache_bytes_(max_cache_bytes) {
  CHECK(preferred_max_cache_bytes_);
}

GraphiteImageProvider::~GraphiteImageProvider() = default;

sk_sp<SkImage> GraphiteImageProvider::findOrCreate(
    skgpu::graphite::Recorder* recorder,
    const SkImage* image,
    SkImage::RequiredProperties required_props) {
  // If no mipmaps are required, check to see if we have a mipmapped version
  // anyway - since it can be used in that case.
  // TODO: we could get fancy and, if ever a mipmapped key eclipsed a
  // non-mipmapped key, we could remove the hidden non-mipmapped key/image
  // from the cache.
  if (!required_props.fMipmapped) {
    auto mipmapped_props = required_props;
    mipmapped_props.fMipmapped = true;
    ImageKey mipmapped_key(image, mipmapped_props);
    auto result = cache_.find(mipmapped_key);
    if (result != cache_.end()) {
      return result->second;
    }
  }

  ImageKey key(image, required_props);

  // Check whether this image has an entry in the cache and return it if so.
  auto result = cache_.find(key);
  if (result != cache_.end()) {
    return result->second;
  }

  // Create a Graphite-backed image for `image`.
  sk_sp<SkImage> new_image =
      SkImages::TextureFromImage(recorder, image, required_props);
  if (!new_image) {
    return nullptr;
  }

  // Add the just-created Graphite-backed image to the cache.
  size_t new_image_bytes = new_image->textureSize();
  PurgeCacheIfNecessaryToAllowForNewImage(new_image_bytes);
  auto [iter, success] = cache_.insert({key, new_image});
  CHECK(success);

  // Update the current size of the cache, which should not have overflowed its
  // max size unless the new image is itself larger than the maximum allowed
  // size.
  current_cache_bytes_ += new_image_bytes;
  CHECK(current_cache_bytes_ <= preferred_max_cache_bytes_ ||
        current_cache_bytes_ == new_image_bytes);

  return new_image;
}

void GraphiteImageProvider::PurgeCacheIfNecessaryToAllowForNewImage(
    size_t new_bytes) {
  // Check for the corner case of the new image being larger than the maximum
  // allowed size, in which case we just empty the cache.
  if (new_bytes > preferred_max_cache_bytes_) {
    cache_.clear();
    current_cache_bytes_ = 0;
    return;
  }

  // Remove entries from the cache until it is small enough to admit `new_bytes`
  // while remaining at or below the maximum allowed size.
  // TODO(crbug.com/1457525): A smarter strategy such as LRU could be used here.
  while (current_cache_bytes_ + new_bytes > preferred_max_cache_bytes_) {
    auto image = cache_.begin()->second;
    current_cache_bytes_ -= image->textureSize();
    cache_.erase(cache_.begin());
  }
}

}  // namespace gpu
