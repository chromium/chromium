// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_bindings.h"

namespace base::android {
class ScopedHardwareBufferFenceSync;
}  // namespace base::android

namespace gpu {
class TextureOwner;
class TextureBase;

// This class lets AndroidVideoImageBacking draw video frames.
class GPU_GLES2_EXPORT StreamTextureSharedImageInterface
    : public base::RefCounted<StreamTextureSharedImageInterface> {
 public:
  // Release the underlying resources. This should be called when the image is
  // not longer valid or the context is lost.
  virtual void ReleaseResources() = 0;

  // Update texture image to the most recent frame.
  virtual void UpdateAndBindTexImage() = 0;

  virtual bool HasTextureOwner() const = 0;
  virtual TextureBase* GetTextureBase() const = 0;

  // Notify the texture of overlay decision, When overlay promotion is true,
  // this also sets the bounds of where the overlay is.
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
  // Render the video frame into an overlay plane. Should only be called after
  // the overlay promotion. Return true if it could render to overlay correctly.
  virtual bool RenderToOverlay() = 0;

  // Whether TextureOwner's implementation binds texture to TextureOwner owned
  // texture_id during the texture update.
  virtual bool TextureOwnerBindsTextureOnUpdate() = 0;

  // Provides the buffer backing this image, if it is backed by an
  // AHardwareBuffer. The ScopedHardwareBuffer returned may include a fence
  // which will be signaled when all pending work for the buffer has been
  // finished and it can be safely read from.
  // The buffer is guaranteed to be valid until the lifetime of the object
  // returned.
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() = 0;

 protected:
  virtual ~StreamTextureSharedImageInterface() = default;

 private:
  friend class base::RefCounted<StreamTextureSharedImageInterface>;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
