// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_

#include "base/containers/lru_cache.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkTiledImageUtils.h"
#include "third_party/skia/include/gpu/graphite/ImageProvider.h"

namespace gpu {

// This class is used by Graphite to create Graphite-backed SkImages from non-
// Graphite-backed SkImages. It is given to a Graphite Recorder on creation. If
// no ImageProvider is given to a Recorder, then any non-Graphite-backed SkImage
// draws on that Recorder will fail.
//
// See https://crsrc.org/c/third_party/skia/include/gpu/graphite/ImageProvider.h
// for details on Skia's requirements for ImageProvider.
class GraphiteImageProvider : public skgpu::graphite::ImageProvider {
 public:
  explicit GraphiteImageProvider(size_t max_cache_bytes);
  ~GraphiteImageProvider() override;

  sk_sp<SkImage> findOrCreate(
      skgpu::graphite::Recorder* recorder,
      const SkImage* image,
      SkImage::RequiredProperties required_props) override;

  void PurgeImagesNotUsedSince(base::TimeDelta last_use_delta);

  void ClearImageCache();

  size_t CurrentSizeInBytes() const { return current_cache_bytes_; }

 private:
  // This class caches images based on a Skia utility that maps images to keys
  // intended to be used for this purpose.
  class ImageKey {
   public:
    ImageKey(const SkImage* image,
             SkImage::RequiredProperties required_properties)
        : required_properties_(required_properties) {
      SkTiledImageUtils::GetImageKeyValues(image, values_.data());
    }

    bool operator==(const ImageKey& other) const {
      return std::tie(values_, required_properties_) ==
             std::tie(other.values_, other.required_properties_);
    }
    bool operator<(const ImageKey& other) const {
      return std::tie(values_, required_properties_) <
             std::tie(other.values_, other.required_properties_);
    }

   private:
    std::array<uint32_t, SkTiledImageUtils::kNumImageKeyValues> values_;
    SkImage::RequiredProperties required_properties_;
  };

  struct ImageHolder {
    explicit ImageHolder(sk_sp<SkImage>);
    ImageHolder(ImageHolder&&);
    ImageHolder& operator=(ImageHolder&&);
    ~ImageHolder();

    sk_sp<SkImage> image;
    base::TimeTicks last_use_time;
  };

  // Ensures that the cache is reduced to a size such that it can support the
  // addition of `new_bytes` while ideally remaining <=
  // `preferred_max_cache_bytes_` overall. The only case in which the latter
  // won't be true is if `new_bytes` > `preferred_max_cache_bytes_`, in which
  // case the cache will be emptied.
  void PurgeCacheIfNecessaryToAllowForNewImage(size_t new_bytes);

  using ImageCache = base::LRUCache<ImageKey, ImageHolder>;
  ImageCache cache_{ImageCache::NO_AUTO_EVICT};

  // The current size of the cache in bytes.
  size_t current_cache_bytes_ = 0;

  // The preferred maximum size of the cache in bytes. This will be honored
  // except in cases where a single image is larger than this size, in which
  // case the single image will be retained in the cache.
  size_t preferred_max_cache_bytes_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_IMAGE_PROVIDER_H_
