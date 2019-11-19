// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_
#define MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_image.h"

namespace media {

class Texture2DWrapper;

// PictureBuffer that owns Chrome Textures to display it, and keep a reference
// to the D3D texture that backs the image.
//
// This is created and owned on the decoder thread.  While currently that's the
// gpu main thread, we still keep the decoder parts separate from the chrome GL
// parts, in case that changes.
//
// This is refcounted so that VideoFrame can hold onto it indirectly.  While
// the chrome Texture is sufficient to keep the pictures renderable, we still
// need to guarantee that the client has time to use the mailbox.  Once it
// does so, it would be fine if this were destroyed.  Technically, only the
// GpuResources have to be retained until the mailbox is used, but we just
// retain the whole thing.
class MEDIA_GPU_EXPORT D3D11PictureBuffer
    : public base::RefCountedThreadSafe<D3D11PictureBuffer> {
 public:
  // |texture_wrapper| is responsible for controlling mailbox access to
  // the ID3D11Texture2D,
  // |level| is the picturebuffer index inside the Array-type ID3D11Texture2D.
  D3D11PictureBuffer(std::unique_ptr<Texture2DWrapper> texture_wrapper,
                     gfx::Size size,
                     size_t level);

  bool Init(GetCommandBufferHelperCB get_helper_cb,
            ComD3D11VideoDevice video_device,
            const GUID& decoder_guid,
            std::unique_ptr<MediaLog> media_log);

  // Set the contents of a mailbox holder array, return true if successful.
  bool ProcessTexture(MailboxHolderArray* mailbox_dest);
  ComD3D11Texture2D Texture() const;

  const gfx::Size& size() const { return size_; }
  size_t level() const { return level_; }

  // Is this PictureBuffer backing a VideoFrame right now?
  bool in_client_use() const { return in_client_use_; }

  // Is this PictureBuffer holding an image that's in use by the decoder?
  bool in_picture_use() const { return in_picture_use_; }

  void set_in_client_use(bool use) { in_client_use_ = use; }
  void set_in_picture_use(bool use) { in_picture_use_ = use; }

  const ComD3D11VideoDecoderOutputView& output_view() const {
    return output_view_;
  }

  // Shouldn't be here, but simpler for now.
  base::TimeDelta timestamp_;

 private:
  ~D3D11PictureBuffer();
  friend class base::RefCountedThreadSafe<D3D11PictureBuffer>;

  std::unique_ptr<Texture2DWrapper> texture_wrapper_;
  gfx::Size size_;
  bool in_picture_use_ = false;
  bool in_client_use_ = false;
  size_t level_;

  ComD3D11VideoDecoderOutputView output_view_;

  DISALLOW_COPY_AND_ASSIGN(D3D11PictureBuffer);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_PICTURE_BUFFER_H_
