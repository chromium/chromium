// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_CODEC_SURFACE_BUNDLE_H_
#define MEDIA_GPU_ANDROID_CODEC_SURFACE_BUNDLE_H_

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "media/base/android/android_overlay.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace media {

// CodecSurfaceBundle is a Java surface, and the TextureOwner or Overlay that
// backs it.
//
// Once a MediaCodec is configured with an output surface, the corresponding
// CodecSurfaceBundle should be kept alive as long as the codec to prevent
// crashes due to the codec losing its output surface.
class MEDIA_GPU_EXPORT CodecSurfaceBundle
    : public base::RefCountedDeleteOnSequence<CodecSurfaceBundle> {
 public:
  using ScheduleLayoutCB = base::RepeatingCallback<void(const gfx::Rect&)>;

  // Create an empty bundle to be manually populated.
  CodecSurfaceBundle();
  explicit CodecSurfaceBundle(std::unique_ptr<AndroidOverlay> overlay);
  explicit CodecSurfaceBundle(scoped_refptr<gpu::TextureOwner> texture_owner);

  const base::android::JavaRef<jobject>& GetJavaSurface() const;

  // Returns a callback that can be used to position this overlay.  It must be
  // called on the correct thread for the overlay.  It will not keep a ref to
  // |this|; the cb will do nothing if |this| is destroyed.
  ScheduleLayoutCB GetScheduleLayoutCB();

  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator()
      const {
    return codec_buffer_wait_coordinator_;
  }
  AndroidOverlay* overlay() const { return overlay_.get(); }

 private:
  ~CodecSurfaceBundle();
  friend class base::RefCountedDeleteOnSequence<CodecSurfaceBundle>;
  friend class base::DeleteHelper<CodecSurfaceBundle>;

  void ScheduleLayout(const gfx::Rect& rect);

  // The Overlay or TextureOwner.
  std::unique_ptr<AndroidOverlay> overlay_;

  // |codec_buffer_wait_coordinator_| owns the TextureOwner.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;

  // The Java surface for |codec_buffer_wait_coordinator_|'s TextureOwner.
  gl::ScopedJavaSurface texture_owner_surface_;

  // The last updated layout rect position for the |overlay|.
  gfx::Rect layout_rect_;

  base::WeakPtrFactory<CodecSurfaceBundle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CodecSurfaceBundle);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_CODEC_SURFACE_BUNDLE_H_
