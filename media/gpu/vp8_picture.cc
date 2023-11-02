// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp8_picture.h"

namespace media {

VP8Picture::VP8Picture() : frame_hdr(new Vp8FrameHeader()) {}

VP8Picture::~VP8Picture() = default;

V4L2VP8Picture* VP8Picture::AsV4L2VP8Picture() {
  return nullptr;
}

VaapiVP8Picture* VP8Picture::AsVaapiVP8Picture() {
  return nullptr;
}

}  // namespace media
