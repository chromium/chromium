// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_

#include "media/gpu/windows/d3d11_video_decoder_wrapper.h"

namespace media {

class CodecPicture;
class D3D11PictureBuffer;

// Acts as a parent class for the D3D11VideoDecoder to expose
// required methods to D3D11VideoAccelerators.
class D3D11VideoDecoderClient {
 public:
  virtual D3D11PictureBuffer* GetPicture() = 0;
  virtual void UpdateTimestamp(D3D11PictureBuffer* picture_buffer) = 0;
  virtual bool OutputResult(const CodecPicture* picture,
                            D3D11PictureBuffer* picture_buffer) = 0;
  // Get the pointer of the D3DVideoDecoderWrapper instance. Callers should not
  // store the return value since the wrapper may change over time.
  virtual D3DVideoDecoderWrapper* GetWrapper() = 0;

 protected:
  virtual ~D3D11VideoDecoderClient() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_CLIENT_H_
