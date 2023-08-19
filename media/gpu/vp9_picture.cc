// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_picture.h"

#include <memory>

#include "base/memory/scoped_refptr.h"

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

  // No members of VP9Picture to copy. `frame_hdr` will be replaced and
  // `metadata_for_encoding` is not used during decoding.

  // Copy members of CodecPicture.
  // `decrypt_config` is not used with VP9.
  ret->set_bitstream_id(bitstream_id());
  ret->set_visible_rect(visible_rect());
  ret->set_colorspace(get_colorspace());
  ret->set_hdr_metadata(hdr_metadata());

  return ret;
}

scoped_refptr<VP9Picture> VP9Picture::CreateDuplicate() {
  return base::MakeRefCounted<VP9Picture>();
}

}  // namespace media
