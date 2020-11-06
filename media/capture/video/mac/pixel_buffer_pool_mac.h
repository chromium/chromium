// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_PIXEL_BUFFER_POOL_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_PIXEL_BUFFER_POOL_MAC_H_

#import <VideoToolbox/VideoToolbox.h>
#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/optional.h"
#include "media/capture/capture_export.h"

namespace media {

class CAPTURE_EXPORT PixelBufferPool {
 public:
  // All buffers created by the pool will have the specified properties.
  // If specified, the pool allows at most |max_buffers| to exist at a time by
  // having CreateBuffer() returns null if this threshold would be exceeded.
  //
  // If an unsupportd |format| is specified, or the pool cannot be created for
  // unknown reasons, null is returned.
  static std::unique_ptr<PixelBufferPool> Create(
      OSType format,
      int width,
      int height,
      base::Optional<size_t> max_buffers);
  ~PixelBufferPool();

  // Creates a new buffer from the pool, or returns null if |max_buffers_| would
  // be exceeded. The underlying buffers may be recycled.
  //
  // Freeing all buffer references returns the underlying buffer to the pool. In
  // order to free memory, you must both release all buffers and call Flush() or
  // delete the pool. It is safe for a buffer to outlive its pool.
  //
  // Retaining a pixel buffer and preventing it from returning to the pool can
  // be done either by keeping a reference directly to the CVPixelBuffer, e.g.
  // with a base::ScopedCFTypeRef<CVPixelBufferRef>, or by incrementing the use
  // count of the IOSurface, i.e. with IOSurfaceIncrementUseCount().
  //
  // WARNING: Retaining references to the pixel buffer's IOSurface (e.g. with
  // base::ScopedCFTypeRef<IOSurfaceRef>) without incrementing its use count
  // does NOT prevent it from being recycled!
  base::ScopedCFTypeRef<CVPixelBufferRef> CreateBuffer();

  // Frees the memory of any released buffers returned to the pool.
  void Flush();

 private:
  friend std::unique_ptr<PixelBufferPool> std::make_unique<PixelBufferPool>(
      base::ScopedCFTypeRef<CVPixelBufferPoolRef>&& buffer_pool,
      base::Optional<size_t>&& max_buffers);

  PixelBufferPool(base::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool,
                  base::Optional<size_t> max_buffers);

  base::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool_;
  const base::Optional<size_t> max_buffers_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_PIXEL_BUFFER_POOL_MAC_H_
