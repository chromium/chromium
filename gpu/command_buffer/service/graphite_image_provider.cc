// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/skia/include/gpu/graphite/Image.h"

namespace gpu {

GraphiteImageProvider::ImageHolder::ImageHolder(sk_sp<SkImage> image)
    : image(std::move(image)), last_use_time(base::TimeTicks::Now()) {}
GraphiteImageProvider::ImageHolder::ImageHolder(ImageHolder&&) = default;
GraphiteImageProvider::ImageHolder&
GraphiteImageProvider::ImageHolder::operator=(ImageHolder&&) = default;
GraphiteImageProvider::ImageHolder::~ImageHolder() = default;

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
    auto result = cache_.Get(mipmapped_key);
    if (result != cache_.end()) {
      result->second.last_use_time = base::TimeTicks::Now();
      return result->second.image;
    }
  }

  ImageKey key(image, required_props);

  // Check whether this image has an entry in the cache and return it if so.
  auto it = cache_.Get(key);
  bool hit_in_cache = it != cache_.end();
  UMA_HISTOGRAM_BOOLEAN("Gpu.Graphite.GraphiteImageProviderAccessHitInCache",
                        hit_in_cache);

  if (hit_in_cache) {
    it->second.last_use_time = base::TimeTicks::Now();
    return it->second.image;
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
  cache_.Put(key, ImageHolder(new_image));

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
    ClearImageCache();
    return;
  }

  // Remove entries from the cache in LRU order until it is small enough to
  // admit `new_bytes` while remaining at or below the maximum allowed size.
  while (current_cache_bytes_ + new_bytes > preferred_max_cache_bytes_) {
    CHECK(!cache_.empty());
    current_cache_bytes_ -= cache_.rbegin()->second.image->textureSize();
    cache_.Erase(cache_.rbegin());
  }
}

void GraphiteImageProvider::PurgeImagesNotUsedSince(
    base::TimeDelta last_use_delta) {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!cache_.empty() &&
         cache_.rbegin()->second.last_use_time + last_use_delta < now) {
    current_cache_bytes_ -= cache_.rbegin()->second.image->textureSize();
    cache_.Erase(cache_.rbegin());
  }
}

void GraphiteImageProvider::ClearImageCache() {
  cache_.Clear();
  current_cache_bytes_ = 0;
}

}  // namespace gpu
