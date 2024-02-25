// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MAYBE_RENDER_EARLY_MANAGER_H_
#define MEDIA_GPU_ANDROID_MAYBE_RENDER_EARLY_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/android/codec_image.h"  // For CodecImage::BlockingMode
#include "media/gpu/media_gpu_export.h"

namespace media {
class CodecImageHolder;
class CodecSurfaceBundle;

// Manager for rendering images speculatively.  Tries to advance images to the
// back buffer, then to the front buffer, once those slots are unoccupied.
class MEDIA_GPU_EXPORT MaybeRenderEarlyManager {
 public:
  MaybeRenderEarlyManager() = default;

  MaybeRenderEarlyManager(const MaybeRenderEarlyManager&) = delete;
  MaybeRenderEarlyManager& operator=(const MaybeRenderEarlyManager&) = delete;

  virtual ~MaybeRenderEarlyManager() = default;

  // Sets the surface bundle that future images will use.
  virtual void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) = 0;

  // Adds |codec_image_holder| and tries to render early.
  //
  // Note that CodecImage should be some other abstraction that handles
  // front / backbuffer rendering.  However, for now, CodecImage does that.
  virtual void AddCodecImage(
      scoped_refptr<CodecImageHolder> codec_image_holder) = 0;

  // Try to render codec images early.  It's okay if no work can be done.
  virtual void MaybeRenderEarly() = 0;

  // Create a default instance that uses |gpu_task_runner| to render early.
  // Note that the returned object should be accessed from the thread that
  // created it.
  static std::unique_ptr<MaybeRenderEarlyManager> Create(
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      scoped_refptr<gpu::RefCountedLock> drdc_lock);
};

namespace internal {

// Tries to render CodecImages to their backing surfaces when it's valid to do
// so. This lets us release codec buffers back to their codecs as soon as
// possible so that decoding can progress smoothly.
// Templated on the image type for testing.
template <typename Image>
void MEDIA_GPU_EXPORT MaybeRenderEarly(
    std::vector<raw_ptr<Image, VectorExperimental>>* image_vector_ptr) {
  auto& images = *image_vector_ptr;
  if (images.empty())
    return;

  // Find the latest image rendered to the front buffer (if any).
  std::optional<size_t> front_buffer_index;
  for (int i = images.size() - 1; i >= 0; --i) {
    if (images[i]->was_rendered_to_front_buffer()) {
      front_buffer_index = i;
      break;
    }
  }

  // If there's no image in the front buffer we can safely render one.
  if (!front_buffer_index.has_value()) {
    // Iterate until we successfully render one to skip over invalidated images.
    for (size_t i = 0; i < images.size(); ++i) {
      if (images[i]->RenderToFrontBuffer()) {
        front_buffer_index = i;
        break;
      }
    }
    // If we couldn't render anything there's nothing more to do.
    if (!front_buffer_index.has_value())
      return;
  }

  // Try to render the image following the front buffer to the back buffer.
  size_t back_buffer_index = *front_buffer_index + 1;
  if (back_buffer_index < images.size() &&
      images[back_buffer_index]->is_texture_owner_backed()) {
    // Try to render to the back buffer, but don't wait for any previous frame.
    // While this does make it more likely that we'll have to wait the next time
    // we draw, it does prevent us from waiting on frames we don't plan to draw.
    images[back_buffer_index]->RenderToTextureOwnerBackBuffer();
  }
}

}  // namespace internal

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MAYBE_RENDER_EARLY_MANAGER_H_
