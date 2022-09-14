// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_picture.h"

#include <memory>

namespace media {

VP9Picture::VP9Picture() : frame_hdr(new Vp9FrameHeader()) {}

VP9Picture::~VP9Picture() = default;

V4L2VP9Picture* VP9Picture::AsV4L2VP9Picture() {
  return nullptr;
}

VaapiVP9Picture* VP9Picture::AsVaapiVP9Picture() {
  return nullptr;
}

scoped_refptr<VP9Picture> VP9Picture::Duplicate() {
  scoped_refptr<VP9Picture> ret = CreateDuplicate();
  if (ret == nullptr)
    return nullptr;

  // Copy member of VP9Picture.
  ret->frame_hdr = std::make_unique<Vp9FrameHeader>();
  memcpy(ret->frame_hdr.get(), frame_hdr.get(), sizeof(Vp9FrameHeader));

  // Copy member of CodecPicture.
  // Note that decrypt_config_ is not used in here, so skip copying it.
  ret->set_bitstream_id(bitstream_id());
  ret->set_visible_rect(visible_rect());
  ret->set_colorspace(get_colorspace());

  return ret;
}

scoped_refptr<VP9Picture> VP9Picture::CreateDuplicate() {
  return nullptr;
}

}  // namespace media
