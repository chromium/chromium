// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_picture.h"

namespace media {

D3D11VP9Picture::D3D11VP9Picture(D3D11PictureBuffer* picture_buffer,
                                 D3D11VideoDecoderClient* client)
    : picture_buffer_(picture_buffer),
      client_(client),
      picture_index_(picture_buffer_->picture_index()) {
  picture_buffer_->set_in_picture_use(true);
}

D3D11VP9Picture::~D3D11VP9Picture() {
  picture_buffer_->set_in_picture_use(false);
}

scoped_refptr<VP9Picture> D3D11VP9Picture::CreateDuplicate() {
  // We've already sent off the base frame for rendering, so we can just stamp
  // |picture_buffer_| with the updated timestamp.
  client_->UpdateTimestamp(picture_buffer_);
  return this;
}

}  // namespace media
