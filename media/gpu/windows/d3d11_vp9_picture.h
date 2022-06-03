// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VP9_PICTURE_H_
#define MEDIA_GPU_WINDOWS_D3D11_VP9_PICTURE_H_

#include "media/gpu/vp9_picture.h"

#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"

namespace media {

class D3D11PictureBuffer;

class D3D11VP9Picture : public VP9Picture {
 public:
  explicit D3D11VP9Picture(D3D11PictureBuffer* picture_buffer,
                           D3D11VideoDecoderClient* client);

  D3D11PictureBuffer* picture_buffer() const { return picture_buffer_; }

  size_t picture_index() const { return picture_index_; }

 protected:
  ~D3D11VP9Picture() override;

  scoped_refptr<VP9Picture> CreateDuplicate() override;

 private:
  D3D11PictureBuffer* picture_buffer_;
  D3D11VideoDecoderClient* client_;
  size_t picture_index_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VP9_PICTURE_H_
