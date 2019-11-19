// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_

#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class TextureOwner;

namespace gles2 {
class Texture;
}  // namespace gles2

// This class is a specialized GLImage that lets SharedImageVideo draw video
// frames.
class GPU_GLES2_EXPORT StreamTextureSharedImageInterface
    : public gles2::GLStreamTextureImage {
 public:
  // Release the underlying resources. This should be called when the image is
  // not longer valid or the context is lost.
  virtual void ReleaseResources() = 0;

  // Whether the StreamTextureSharedImageInterface is accounting for gpu memory
  // or not.
  virtual bool IsUsingGpuMemory() const = 0;

  // Update the texture image to the most recent frame and bind it to the
  // texture.
  virtual void UpdateAndBindTexImage() = 0;
  virtual bool HasTextureOwner() const = 0;
  virtual gles2::Texture* GetTexture() const = 0;

  // Notify the texture of overlay decision, When overlay promotion is true,
  // this also sets the bounds of where the overlay is.
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
  // Render the video frame into an overlay plane. Should only be called after
  // the overlay promotion. Return true if it could render to overlay correctly.
  virtual bool RenderToOverlay() = 0;

 protected:
  ~StreamTextureSharedImageInterface() override = default;

  enum class BindingsMode {
    // Ensures that the TextureOwner's texture is bound to the latest image, if
    // it requires explicit binding.
    kEnsureTexImageBound,

    // Updates the current image but does not bind it. If updating the image
    // implicitly binds the texture, the current bindings will be restored.
    kRestoreIfBound,

    // Updates the current image but does not bind it. If updating the image
    // implicitly binds the texture, the current bindings will not be restored.
    kDontRestoreIfBound
  };
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_STREAM_TEXTURE_SHARED_IMAGE_INTERFACE_H_
